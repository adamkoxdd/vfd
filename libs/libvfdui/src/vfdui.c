#define _POSIX_C_SOURCE 200809L
#include "vfdui/vfdui.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <pango/pangocairo.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct VfdWindow {
    Display *display;
    int screen;
    Window xwindow;
    Atom wm_delete;
    cairo_surface_t *surface;
    cairo_t *cr;
    VfdTheme theme;
    bool open;
    int width;
    int height;
    int mouse_x;
    int mouse_y;
    bool mouse_down;
    double start_time;
    double last_time;
    double delta;
    int clip_depth;
};

static double monotonic_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

static void set_color(cairo_t *cr, VfdColor c) {
    cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a);
}

static bool inside(VfdRect r, int x, int y) {
    return x >= r.x && x <= r.x + r.width &&
           y >= r.y && y <= r.y + r.height;
}

static double clampd(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void rounded_rect(cairo_t *cr, VfdRect r, double radius) {
    double x = r.x;
    double y = r.y;
    double w = r.width;
    double h = r.height;
    double rr = fmin(radius, fmin(w, h) / 2.0);

    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - rr, y + rr, rr, -G_PI / 2, 0);
    cairo_arc(cr, x + w - rr, y + h - rr, rr, 0, G_PI / 2);
    cairo_arc(cr, x + rr, y + h - rr, rr, G_PI / 2, G_PI);
    cairo_arc(cr, x + rr, y + rr, rr, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
}

static void rebuild_surface(VfdWindow *window) {
    if (window->cr) cairo_destroy(window->cr);
    if (window->surface) cairo_surface_destroy(window->surface);

    window->surface = cairo_xlib_surface_create(
        window->display,
        window->xwindow,
        DefaultVisual(window->display, window->screen),
        window->width,
        window->height
    );
    window->cr = cairo_create(window->surface);
}

VfdColor vfd_color_rgba(double r, double g, double b, double a) {
    return (VfdColor){r, g, b, a};
}

VfdColor vfd_color_hex(const char *hex) {
    unsigned int r = 0, g = 0, b = 0, a = 255;
    if (!hex) return vfd_color_rgba(1, 1, 1, 1);
    if (hex[0] == '#') hex++;

    size_t len = strlen(hex);
    if (len == 6) {
        sscanf(hex, "%02x%02x%02x", &r, &g, &b);
    } else if (len == 8) {
        sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
    }

    return vfd_color_rgba(
        r / 255.0,
        g / 255.0,
        b / 255.0,
        a / 255.0
    );
}

VfdTheme vfd_theme_default(void) {
    return (VfdTheme){
        .background = {0.039, 0.039, 0.059, 1.0},
        .panel = {0.059, 0.059, 0.102, 1.0},
        .phosphor = {0.608, 0.498, 0.831, 1.0},
        .glow = {0.725, 0.612, 1.0, 1.0},
        .dim = {0.310, 0.255, 0.427, 1.0},
        .danger = {0.92, 0.35, 0.48, 1.0},
        .success = {0.40, 0.82, 0.62, 1.0},
        .border_width = 2.0,
        .corner_radius = 8.0,
        .glow_strength = 0.18,
        .scanline_alpha = 0.025,
        .spacing = 12,
        .font_size = 14,
        .title_size = 19,
        .font_family = "monospace",
        .scanlines = true,
    };
}

VfdWindow *vfd_window_create(
    const char *title,
    int width,
    int height,
    const VfdTheme *theme
) {
    VfdWindow *window = calloc(1, sizeof(*window));
    if (!window) return NULL;

    window->display = XOpenDisplay(NULL);
    if (!window->display) {
        free(window);
        return NULL;
    }

    window->screen = DefaultScreen(window->display);
    window->width = width;
    window->height = height;
    window->theme = theme ? *theme : vfd_theme_default();
    window->open = true;
    window->start_time = monotonic_seconds();
    window->last_time = window->start_time;

    window->xwindow = XCreateSimpleWindow(
        window->display,
        RootWindow(window->display, window->screen),
        0, 0,
        (unsigned int)width,
        (unsigned int)height,
        0,
        BlackPixel(window->display, window->screen),
        BlackPixel(window->display, window->screen)
    );

    XStoreName(window->display, window->xwindow, title ? title : "VFD");
    XSelectInput(
        window->display,
        window->xwindow,
        ExposureMask |
        StructureNotifyMask |
        KeyPressMask |
        PointerMotionMask |
        ButtonPressMask |
        ButtonReleaseMask
    );

    window->wm_delete = XInternAtom(window->display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window->display, window->xwindow, &window->wm_delete, 1);

    XMapWindow(window->display, window->xwindow);
    rebuild_surface(window);
    return window;
}

void vfd_window_destroy(VfdWindow *window) {
    if (!window) return;
    while (window->clip_depth-- > 0) cairo_restore(window->cr);
    if (window->cr) cairo_destroy(window->cr);
    if (window->surface) cairo_surface_destroy(window->surface);
    if (window->display && window->xwindow) {
        XDestroyWindow(window->display, window->xwindow);
    }
    if (window->display) XCloseDisplay(window->display);
    free(window);
}

bool vfd_window_is_open(const VfdWindow *window) {
    return window && window->open;
}

void vfd_window_close(VfdWindow *window) {
    if (window) window->open = false;
}

static VfdKey translate_key(KeySym sym, uint32_t *codepoint) {
    *codepoint = 0;
    switch (sym) {
        case XK_Escape: return VFD_KEY_ESCAPE;
        case XK_Return: return VFD_KEY_ENTER;
        case XK_space: return VFD_KEY_SPACE;
        case XK_Left: return VFD_KEY_LEFT;
        case XK_Right: return VFD_KEY_RIGHT;
        case XK_Up: return VFD_KEY_UP;
        case XK_Down: return VFD_KEY_DOWN;
        case XK_Tab: return VFD_KEY_TAB;
        case XK_BackSpace: return VFD_KEY_BACKSPACE;
        default:
            if (sym >= XK_space && sym <= XK_asciitilde) {
                *codepoint = (uint32_t)sym;
                return VFD_KEY_CHARACTER;
            }
            return VFD_KEY_NONE;
    }
}

VfdEvent vfd_window_poll(VfdWindow *window) {
    VfdEvent event = {0};
    if (!window) {
        event.quit = true;
        return event;
    }

    event.width = window->width;
    event.height = window->height;
    event.mouse_x = window->mouse_x;
    event.mouse_y = window->mouse_y;

    while (XPending(window->display)) {
        XEvent xev;
        XNextEvent(window->display, &xev);

        switch (xev.type) {
            case ClientMessage:
                if ((Atom)xev.xclient.data.l[0] == window->wm_delete) {
                    window->open = false;
                    event.quit = true;
                }
                break;
            case ConfigureNotify:
                if (xev.xconfigure.width != window->width ||
                    xev.xconfigure.height != window->height) {
                    window->width = xev.xconfigure.width;
                    window->height = xev.xconfigure.height;
                    rebuild_surface(window);
                    event.resized = true;
                    event.width = window->width;
                    event.height = window->height;
                }
                break;
            case MotionNotify:
                window->mouse_x = xev.xmotion.x;
                window->mouse_y = xev.xmotion.y;
                event.mouse_moved = true;
                event.mouse_x = window->mouse_x;
                event.mouse_y = window->mouse_y;
                break;
            case ButtonPress:
                window->mouse_down = true;
                window->mouse_x = xev.xbutton.x;
                window->mouse_y = xev.xbutton.y;
                event.mouse_x = window->mouse_x;
                event.mouse_y = window->mouse_y;
                if (xev.xbutton.button == Button4) event.scroll_y = 1;
                else if (xev.xbutton.button == Button5) event.scroll_y = -1;
                else event.mouse_pressed = true;
                break;
            case ButtonRelease:
                window->mouse_down = false;
                window->mouse_x = xev.xbutton.x;
                window->mouse_y = xev.xbutton.y;
                event.mouse_released = true;
                event.mouse_x = window->mouse_x;
                event.mouse_y = window->mouse_y;
                break;
            case KeyPress: {
                KeySym sym = XLookupKeysym(&xev.xkey, 0);
                event.key = translate_key(sym, &event.codepoint);
                if (event.key == VFD_KEY_ESCAPE) event.quit = true;
                break;
            }
        }
    }

    return event;
}

void vfd_window_begin(VfdWindow *window) {
    if (!window) return;
    double now = monotonic_seconds();
    window->delta = now - window->last_time;
    window->last_time = now;
    vfd_clear(window);
}

void vfd_window_end(VfdWindow *window) {
    if (!window) return;
    while (window->clip_depth > 0) vfd_scrollview_end(window);
    if (window->theme.scanlines) vfd_scanlines(window);
    cairo_surface_flush(window->surface);
    XFlush(window->display);
}

double vfd_window_time(const VfdWindow *window) {
    return window ? monotonic_seconds() - window->start_time : 0.0;
}

double vfd_window_delta(const VfdWindow *window) {
    return window ? window->delta : 0.0;
}

int vfd_window_width(const VfdWindow *window) {
    return window ? window->width : 0;
}

int vfd_window_height(const VfdWindow *window) {
    return window ? window->height : 0;
}

void vfd_clear(VfdWindow *window) {
    if (!window) return;
    set_color(window->cr, window->theme.background);
    cairo_paint(window->cr);
}

void vfd_scanlines(VfdWindow *window) {
    if (!window) return;
    cairo_save(window->cr);
    cairo_set_source_rgba(
        window->cr,
        window->theme.phosphor.r,
        window->theme.phosphor.g,
        window->theme.phosphor.b,
        window->theme.scanline_alpha
    );
    for (int y = 0; y < window->height; y += 5) {
        cairo_rectangle(window->cr, 0, y, window->width, 1);
    }
    cairo_fill(window->cr);
    cairo_restore(window->cr);
}

void vfd_panel(VfdWindow *window, VfdRect rect, bool filled) {
    if (!window) return;
    cairo_save(window->cr);
    rounded_rect(window->cr, rect, window->theme.corner_radius);

    if (filled) {
        set_color(window->cr, window->theme.panel);
        cairo_fill_preserve(window->cr);
    }

    cairo_set_line_width(window->cr, window->theme.border_width);
    set_color(window->cr, window->theme.dim);
    cairo_stroke(window->cr);
    cairo_restore(window->cr);
}

void vfd_separator(
    VfdWindow *window,
    double x1,
    double y1,
    double x2,
    double y2
) {
    if (!window) return;
    cairo_save(window->cr);
    cairo_set_line_width(window->cr, 1.0);
    set_color(window->cr, window->theme.dim);
    cairo_move_to(window->cr, x1, y1);
    cairo_line_to(window->cr, x2, y2);
    cairo_stroke(window->cr);
    cairo_restore(window->cr);
}

void vfd_label(
    VfdWindow *window,
    const char *text,
    VfdRect rect,
    int size,
    bool bold,
    VfdAlign align,
    VfdColor color
) {
    if (!window || !text) return;

    PangoLayout *layout = pango_cairo_create_layout(window->cr);
    PangoFontDescription *font = pango_font_description_new();

    pango_font_description_set_family(font, window->theme.font_family);
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(
        font,
        bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL
    );

    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, text, -1);
    pango_layout_set_width(layout, (int)(rect.width * PANGO_SCALE));

    pango_layout_set_alignment(
        layout,
        align == VFD_ALIGN_CENTER ? PANGO_ALIGN_CENTER :
        align == VFD_ALIGN_RIGHT ? PANGO_ALIGN_RIGHT :
        PANGO_ALIGN_LEFT
    );

    cairo_save(window->cr);
    set_color(window->cr, color);
    cairo_move_to(window->cr, rect.x, rect.y);
    pango_cairo_show_layout(window->cr, layout);
    cairo_restore(window->cr);

    pango_font_description_free(font);
    g_object_unref(layout);
}

void vfd_title(VfdWindow *window, const char *text, VfdRect rect) {
    if (!window) return;
    vfd_label(
        window, text, rect,
        window->theme.title_size, true,
        VFD_ALIGN_LEFT,
        window->theme.phosphor
    );
}

void vfd_progress(
    VfdWindow *window,
    VfdRect rect,
    double value,
    const char *label
) {
    if (!window) return;
    value = clampd(value, 0.0, 1.0);
    vfd_panel(window, rect, true);

    VfdRect fill = {
        rect.x + 3,
        rect.y + 3,
        (rect.width - 6) * value,
        rect.height - 6
    };

    cairo_save(window->cr);
    rounded_rect(window->cr, fill, window->theme.corner_radius / 2.0);
    set_color(window->cr, window->theme.phosphor);
    cairo_fill(window->cr);
    cairo_restore(window->cr);

    if (label) {
        vfd_label(
            window, label, rect,
            window->theme.font_size, true,
            VFD_ALIGN_CENTER,
            window->theme.glow
        );
    }
}

bool vfd_button(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    const VfdEvent *event
) {
    if (!window || !event) return false;
    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    bool clicked = hover && event->mouse_released;

    cairo_save(window->cr);
    rounded_rect(window->cr, rect, window->theme.corner_radius);

    VfdColor fill = window->theme.panel;
    if (hover) {
        fill.r += window->theme.glow_strength;
        fill.g += window->theme.glow_strength * 0.8;
        fill.b += window->theme.glow_strength;
    }

    set_color(window->cr, fill);
    cairo_fill_preserve(window->cr);
    cairo_set_line_width(window->cr, window->theme.border_width);
    set_color(window->cr, hover ? window->theme.phosphor : window->theme.dim);
    cairo_stroke(window->cr);
    cairo_restore(window->cr);

    vfd_label(
        window, label,
        (VfdRect){
            rect.x,
            rect.y + (rect.height - window->theme.font_size) / 2.0 - 2,
            rect.width,
            rect.height
        },
        window->theme.font_size, true,
        VFD_ALIGN_CENTER,
        hover ? window->theme.glow : window->theme.phosphor
    );

    return clicked;
}

void vfd_meter(
    VfdWindow *window,
    VfdRect rect,
    double value,
    int segments
) {
    if (!window || segments <= 0) return;
    value = clampd(value, 0.0, 1.0);

    double gap = 3.0;
    double segment_width =
        (rect.width - gap * (segments - 1)) / segments;
    int active = (int)round(value * segments);

    for (int i = 0; i < segments; ++i) {
        VfdRect seg = {
            rect.x + i * (segment_width + gap),
            rect.y,
            segment_width,
            rect.height
        };

        cairo_save(window->cr);
        rounded_rect(window->cr, seg, 2.0);
        set_color(
            window->cr,
            i < active ? window->theme.phosphor : window->theme.dim
        );
        cairo_fill(window->cr);
        cairo_restore(window->cr);
    }
}

bool vfd_checkbox(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    bool *value,
    const VfdEvent *event
) {
    if (!window || !value || !event) return false;
    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    bool changed = hover && event->mouse_released;

    if (changed) *value = !*value;

    VfdRect box = {rect.x, rect.y, rect.height, rect.height};
    vfd_panel(window, box, true);

    if (*value) {
        cairo_save(window->cr);
        set_color(window->cr, window->theme.phosphor);
        cairo_set_line_width(window->cr, 3);
        cairo_move_to(window->cr, box.x + 6, box.y + box.height * 0.55);
        cairo_line_to(window->cr, box.x + box.width * 0.42, box.y + box.height - 7);
        cairo_line_to(window->cr, box.x + box.width - 6, box.y + 7);
        cairo_stroke(window->cr);
        cairo_restore(window->cr);
    }

    vfd_label(
        window, label,
        (VfdRect){rect.x + rect.height + 10, rect.y + 2,
                  rect.width - rect.height - 10, rect.height},
        window->theme.font_size, hover,
        VFD_ALIGN_LEFT,
        hover ? window->theme.glow : window->theme.phosphor
    );
    return changed;
}

bool vfd_switch(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    bool *value,
    const VfdEvent *event
) {
    if (!window || !value || !event) return false;
    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    bool changed = hover && event->mouse_released;
    if (changed) *value = !*value;

    double track_w = rect.height * 1.9;
    VfdRect track = {rect.x, rect.y, track_w, rect.height};

    cairo_save(window->cr);
    rounded_rect(window->cr, track, rect.height / 2.0);
    set_color(window->cr, *value ? window->theme.dim : window->theme.panel);
    cairo_fill_preserve(window->cr);
    set_color(window->cr, *value ? window->theme.phosphor : window->theme.dim);
    cairo_set_line_width(window->cr, window->theme.border_width);
    cairo_stroke(window->cr);

    double knob = rect.height - 8;
    double knob_x = *value
        ? track.x + track.width - knob - 4
        : track.x + 4;

    cairo_arc(
        window->cr,
        knob_x + knob / 2.0,
        track.y + track.height / 2.0,
        knob / 2.0,
        0,
        G_PI * 2
    );
    set_color(window->cr, *value ? window->theme.glow : window->theme.dim);
    cairo_fill(window->cr);
    cairo_restore(window->cr);

    vfd_label(
        window, label,
        (VfdRect){
            rect.x + track_w + 12,
            rect.y + 2,
            rect.width - track_w - 12,
            rect.height
        },
        window->theme.font_size, hover,
        VFD_ALIGN_LEFT,
        hover ? window->theme.glow : window->theme.phosphor
    );
    return changed;
}

bool vfd_slider(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    double *value,
    double minimum,
    double maximum,
    const VfdEvent *event
) {
    if (!window || !value || !event || maximum <= minimum) return false;

    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    bool changed = false;

    if ((event->mouse_pressed || event->mouse_moved) &&
        hover && window->mouse_down) {
        double t = (event->mouse_x - rect.x) / rect.width;
        *value = minimum + clampd(t, 0.0, 1.0) * (maximum - minimum);
        changed = true;
    }

    double t = (*value - minimum) / (maximum - minimum);
    t = clampd(t, 0.0, 1.0);

    cairo_save(window->cr);
    cairo_set_line_width(window->cr, 4);
    set_color(window->cr, window->theme.dim);
    cairo_move_to(window->cr, rect.x, rect.y + rect.height / 2.0);
    cairo_line_to(window->cr, rect.x + rect.width, rect.y + rect.height / 2.0);
    cairo_stroke(window->cr);

    set_color(window->cr, window->theme.phosphor);
    cairo_move_to(window->cr, rect.x, rect.y + rect.height / 2.0);
    cairo_line_to(window->cr, rect.x + rect.width * t, rect.y + rect.height / 2.0);
    cairo_stroke(window->cr);

    cairo_arc(
        window->cr,
        rect.x + rect.width * t,
        rect.y + rect.height / 2.0,
        hover ? 9 : 7,
        0,
        G_PI * 2
    );
    set_color(window->cr, hover ? window->theme.glow : window->theme.phosphor);
    cairo_fill(window->cr);
    cairo_restore(window->cr);

    if (label) {
        char buf[128];
        snprintf(buf, sizeof buf, "%s // %.2f", label, *value);
        vfd_label(
            window, buf,
            (VfdRect){rect.x, rect.y - 24, rect.width, 20},
            12, hover, VFD_ALIGN_LEFT,
            hover ? window->theme.glow : window->theme.dim
        );
    }

    return changed;
}

bool vfd_textbox(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    VfdTextboxState *state,
    const VfdEvent *event
) {
    if (!window || !state || !state->buffer || state->capacity == 0 || !event) {
        return false;
    }

    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    bool submitted = false;

    if (event->mouse_released) state->focused = hover;

    if (state->focused) {
        if (event->key == VFD_KEY_BACKSPACE && state->cursor > 0) {
            memmove(
                state->buffer + state->cursor - 1,
                state->buffer + state->cursor,
                state->length - state->cursor + 1
            );
            state->cursor--;
            state->length--;
        } else if (event->key == VFD_KEY_LEFT && state->cursor > 0) {
            state->cursor--;
        } else if (event->key == VFD_KEY_RIGHT &&
                   state->cursor < state->length) {
            state->cursor++;
        } else if (event->key == VFD_KEY_ENTER) {
            submitted = true;
        } else if (event->key == VFD_KEY_CHARACTER &&
                   state->length + 1 < state->capacity &&
                   event->codepoint >= 32 && event->codepoint <= 126) {
            memmove(
                state->buffer + state->cursor + 1,
                state->buffer + state->cursor,
                state->length - state->cursor + 1
            );
            state->buffer[state->cursor] = (char)event->codepoint;
            state->cursor++;
            state->length++;
        }
    }

    vfd_panel(window, rect, true);

    if (label) {
        vfd_label(
            window, label,
            (VfdRect){rect.x + 12, rect.y + 7, rect.width - 24, 18},
            11, true, VFD_ALIGN_LEFT,
            state->focused ? window->theme.glow : window->theme.dim
        );
    }

    char *display = calloc(state->length + 2, 1);
    if (!display) return submitted;

    if (state->password) memset(display, '*', state->length);
    else memcpy(display, state->buffer, state->length);

    if (state->focused &&
        fmod(vfd_window_time(window), 1.0) < 0.5 &&
        state->cursor <= state->length) {
        memmove(
            display + state->cursor + 1,
            display + state->cursor,
            state->length - state->cursor + 1
        );
        display[state->cursor] = '_';
    }

    vfd_label(
        window, display,
        (VfdRect){
            rect.x + 12,
            rect.y + (label ? 25 : 10),
            rect.width - 24,
            rect.height - 12
        },
        window->theme.font_size, false,
        VFD_ALIGN_LEFT,
        window->theme.phosphor
    );

    free(display);
    return submitted;
}

void vfd_scrollview_begin(
    VfdWindow *window,
    VfdRect rect,
    double scroll_y
) {
    if (!window) return;
    cairo_save(window->cr);
    window->clip_depth++;
    cairo_rectangle(window->cr, rect.x, rect.y, rect.width, rect.height);
    cairo_clip(window->cr);
    cairo_translate(window->cr, 0, -scroll_y);
}

void vfd_scrollview_end(VfdWindow *window) {
    if (!window || window->clip_depth <= 0) return;
    cairo_restore(window->cr);
    window->clip_depth--;
}

int vfd_list(
    VfdWindow *window,
    VfdRect rect,
    const char *const *items,
    int item_count,
    int row_height,
    VfdListState *state,
    const VfdEvent *event
) {
    if (!window || !items || item_count <= 0 || !state || !event) return -1;

    if (inside(rect, event->mouse_x, event->mouse_y) && event->scroll_y != 0) {
        state->scroll -= event->scroll_y * row_height * 2;
    }

    double content_height = (double)item_count * row_height;
    double max_scroll = fmax(0.0, content_height - rect.height);
    state->scroll = clampd(state->scroll, 0.0, max_scroll);
    state->hovered = -1;

    vfd_panel(window, rect, true);
    vfd_scrollview_begin(window, rect, state->scroll);

    int clicked = -1;
    for (int i = 0; i < item_count; ++i) {
        VfdRect row = {
            rect.x + 4,
            rect.y + i * row_height + 4,
            rect.width - 8,
            row_height - 4
        };

        int visual_y = (int)(row.y - state->scroll);
        bool hover =
            event->mouse_x >= row.x &&
            event->mouse_x <= row.x + row.width &&
            event->mouse_y >= visual_y &&
            event->mouse_y <= visual_y + row.height &&
            inside(rect, event->mouse_x, event->mouse_y);

        if (hover) state->hovered = i;
        if (hover && event->mouse_released) {
            state->selected = i;
            clicked = i;
        }

        if (i == state->selected || hover) {
            cairo_save(window->cr);
            rounded_rect(window->cr, row, 4);
            set_color(
                window->cr,
                i == state->selected
                    ? vfd_color_rgba(
                        window->theme.phosphor.r,
                        window->theme.phosphor.g,
                        window->theme.phosphor.b,
                        0.18
                    )
                    : vfd_color_rgba(
                        window->theme.glow.r,
                        window->theme.glow.g,
                        window->theme.glow.b,
                        0.08
                    )
            );
            cairo_fill(window->cr);
            cairo_restore(window->cr);
        }

        vfd_label(
            window, items[i],
            (VfdRect){row.x + 10, row.y + 7, row.width - 20, row.height},
            window->theme.font_size,
            i == state->selected,
            VFD_ALIGN_LEFT,
            i == state->selected || hover
                ? window->theme.glow
                : window->theme.phosphor
        );
    }

    vfd_scrollview_end(window);

    if (max_scroll > 0.0) {
        double thumb_h = fmax(24.0, rect.height * rect.height / content_height);
        double thumb_y = rect.y +
            (rect.height - thumb_h) * (state->scroll / max_scroll);

        cairo_save(window->cr);
        rounded_rect(
            window->cr,
            (VfdRect){rect.x + rect.width - 5, thumb_y, 3, thumb_h},
            2
        );
        set_color(window->cr, window->theme.phosphor);
        cairo_fill(window->cr);
        cairo_restore(window->cr);
    }

    return clicked;
}

int vfd_dropdown(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    const char *const *items,
    int item_count,
    VfdDropdownState *state,
    const VfdEvent *event
) {
    if (!window || !items || item_count <= 0 || !state || !event) return -1;
    if (state->selected < 0 || state->selected >= item_count) state->selected = 0;

    bool hover = inside(rect, event->mouse_x, event->mouse_y);
    if (hover && event->mouse_released) state->open = !state->open;

    vfd_panel(window, rect, true);

    char buf[256];
    snprintf(
        buf, sizeof buf,
        "%s // %s %s",
        label ? label : "SELECT",
        items[state->selected],
        state->open ? "▲" : "▼"
    );

    vfd_label(
        window, buf,
        (VfdRect){rect.x + 12, rect.y + 10, rect.width - 24, rect.height},
        window->theme.font_size, hover,
        VFD_ALIGN_LEFT,
        hover ? window->theme.glow : window->theme.phosphor
    );

    int changed = -1;
    if (state->open) {
        VfdRect menu = {
            rect.x,
            rect.y + rect.height + 6,
            rect.width,
            item_count * 34.0 + 8
        };
        vfd_panel(window, menu, true);
        state->hovered = -1;

        for (int i = 0; i < item_count; ++i) {
            VfdRect row = {
                menu.x + 4,
                menu.y + 4 + i * 34,
                menu.width - 8,
                30
            };
            bool row_hover = inside(row, event->mouse_x, event->mouse_y);
            if (row_hover) state->hovered = i;

            if (row_hover && event->mouse_released) {
                state->selected = i;
                state->open = false;
                changed = i;
            }

            if (row_hover || i == state->selected) {
                cairo_save(window->cr);
                rounded_rect(window->cr, row, 4);
                set_color(
                    window->cr,
                    vfd_color_rgba(
                        window->theme.phosphor.r,
                        window->theme.phosphor.g,
                        window->theme.phosphor.b,
                        row_hover ? 0.16 : 0.10
                    )
                );
                cairo_fill(window->cr);
                cairo_restore(window->cr);
            }

            vfd_label(
                window, items[i],
                (VfdRect){row.x + 10, row.y + 6, row.width - 20, row.height},
                window->theme.font_size,
                i == state->selected,
                VFD_ALIGN_LEFT,
                row_hover || i == state->selected
                    ? window->theme.glow
                    : window->theme.phosphor
            );
        }
    }

    return changed;
}

int vfd_tree(
    VfdWindow *window,
    VfdRect rect,
    VfdTreeRow *rows,
    int row_count,
    int row_height,
    int *selected,
    const VfdEvent *event
) {
    if (!window || !rows || row_count <= 0 || !selected || !event) return -1;
    vfd_panel(window, rect, true);

    int clicked = -1;
    int visible_index = 0;
    int hidden_depth = -1;

    for (int i = 0; i < row_count; ++i) {
        if (hidden_depth >= 0) {
            if (rows[i].depth > hidden_depth) continue;
            hidden_depth = -1;
        }

        VfdRect row = {
            rect.x + 4,
            rect.y + 4 + visible_index * row_height,
            rect.width - 8,
            row_height - 4
        };
        visible_index++;

        bool hover = inside(row, event->mouse_x, event->mouse_y);
        if (hover && event->mouse_released) {
            *selected = i;
            clicked = i;
            if (rows[i].has_children) rows[i].expanded = !rows[i].expanded;
        }

        if (*selected == i || hover) {
            cairo_save(window->cr);
            rounded_rect(window->cr, row, 4);
            set_color(
                window->cr,
                vfd_color_rgba(
                    window->theme.phosphor.r,
                    window->theme.phosphor.g,
                    window->theme.phosphor.b,
                    *selected == i ? 0.18 : 0.08
                )
            );
            cairo_fill(window->cr);
            cairo_restore(window->cr);
        }

        char buf[256];
        snprintf(
            buf, sizeof buf,
            "%s%s%s",
            rows[i].has_children ? (rows[i].expanded ? "▼ " : "▶ ") : "  ",
            rows[i].label ? rows[i].label : "",
            ""
        );

        vfd_label(
            window, buf,
            (VfdRect){
                row.x + 8 + rows[i].depth * 18,
                row.y + 6,
                row.width - 16 - rows[i].depth * 18,
                row.height
            },
            window->theme.font_size,
            *selected == i,
            VFD_ALIGN_LEFT,
            hover || *selected == i
                ? window->theme.glow
                : window->theme.phosphor
        );

        if (rows[i].has_children && !rows[i].expanded) {
            hidden_depth = rows[i].depth;
        }
    }

    return clicked;
}
