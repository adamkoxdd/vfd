#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BG_R 0.039
#define BG_G 0.039
#define BG_B 0.059
#define PH_R 0.608
#define PH_G 0.498
#define PH_B 0.831
#define DIM_R 0.235
#define DIM_G 0.196
#define DIM_B 0.333

static void text(cairo_t *cr, const char *s, double x, double y,
                 int size, int bold, double r, double g, double b) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();

    pango_font_description_set_family(font, "monospace");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(
        font, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, s, -1);

    cairo_set_source_rgb(cr, r, g, b);
    cairo_move_to(cr, x, y);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(font);
    g_object_unref(layout);
}

static void centered(cairo_t *cr, const char *s, double y,
                     int size, int bold, int x, int width) {
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();
    int tw = 0, th = 0;

    pango_font_description_set_family(font, "monospace");
    pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
    pango_font_description_set_weight(
        font, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, s, -1);
    pango_layout_get_pixel_size(layout, &tw, &th);

    cairo_set_source_rgb(cr, PH_R, PH_G, PH_B);
    cairo_move_to(cr, x + (width - tw) / 2.0, y);
    pango_cairo_show_layout(cr, layout);

    pango_font_description_free(font);
    g_object_unref(layout);
}

static void draw_monitor(cairo_t *cr, int x, int y, int width, int height,
                         const char *clockbuf, const char *datebuf,
                         const char *host) {
    const int margin = 36;
    const int inner = 28;

    cairo_save(cr);
    cairo_rectangle(cr, x, y, width, height);
    cairo_clip(cr);

    cairo_set_source_rgb(cr, BG_R, BG_G, BG_B);
    cairo_paint(cr);

    cairo_set_source_rgb(cr, DIM_R, DIM_G, DIM_B);
    cairo_set_line_width(cr, 2);
    cairo_rectangle(
        cr,
        x + margin,
        y + margin,
        width - margin * 2,
        height - margin * 2
    );
    cairo_stroke(cr);

    for (int sy = y + margin + 6; sy < y + height - margin; sy += 5) {
        cairo_set_source_rgba(cr, PH_R, PH_G, PH_B, 0.025);
        cairo_rectangle(
            cr,
            x + margin + 6,
            sy,
            width - (margin + 6) * 2,
            1
        );
        cairo_fill(cr);
    }

    text(cr, "VFD SYSTEM // SESSION LOCKED",
         x + margin + inner, y + margin + 22,
         17, 1, PH_R, PH_G, PH_B);

    char node[180];
    snprintf(node, sizeof node, "NODE // %s", host);
    text(cr, node,
         x + margin + inner, y + margin + 50,
         12, 0, DIM_R * 2, DIM_G * 2, DIM_B * 2);

    int clock_size = width < 900 ? 68 : 92;
    centered(cr, clockbuf, y + height * 0.25,
             clock_size, 1, x, width);
    centered(cr, datebuf, y + height * 0.25 + clock_size + 28,
             18, 0, x, width);

    double boxw = width < 900 ? width - 120 : 700;
    if (boxw < 360) boxw = width - 60;
    double boxx = x + (width - boxw) / 2.0;
    double boxy = y + height * 0.58;

    cairo_set_source_rgba(cr, PH_R, PH_G, PH_B, 0.08);
    cairo_rectangle(cr, boxx, boxy, boxw, 112);
    cairo_fill_preserve(cr);

    cairo_set_source_rgb(cr, PH_R, PH_G, PH_B);
    cairo_set_line_width(cr, 2);
    cairo_stroke(cr);

    centered(cr, "PASS // TYPE PASSWORD, THEN PRESS ENTER",
             boxy + 24, 16, 1, x, width);
    centered(cr, "ESC CLEARS INPUT",
             boxy + 62, 12, 0, x, width);

    text(cr, "AUTHENTICATION // i3lock + PAM",
         x + margin + inner, y + height - margin - 34,
         12, 0, DIM_R * 2, DIM_G * 2, DIM_B * 2);

    const char *footer = "VFD DESKTOP";
    PangoLayout *layout = pango_cairo_create_layout(cr);
    PangoFontDescription *font = pango_font_description_new();
    int fw = 0, fh = 0;
    pango_font_description_set_family(font, "monospace");
    pango_font_description_set_absolute_size(font, 12 * PANGO_SCALE);
    pango_font_description_set_weight(font, PANGO_WEIGHT_BOLD);
    pango_layout_set_font_description(layout, font);
    pango_layout_set_text(layout, footer, -1);
    pango_layout_get_pixel_size(layout, &fw, &fh);
    cairo_set_source_rgb(cr, PH_R, PH_G, PH_B);
    cairo_move_to(cr,
                  x + width - margin - inner - fw,
                  y + height - margin - 34);
    pango_cairo_show_layout(cr, layout);
    pango_font_description_free(font);
    g_object_unref(layout);

    cairo_restore(cr);
}

static int render(const char *path) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "vfdlock: cannot open X display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);
    int root_width = DisplayWidth(display, screen);
    int root_height = DisplayHeight(display, screen);

    cairo_surface_t *surface =
        cairo_image_surface_create(CAIRO_FORMAT_RGB24, root_width, root_height);
    cairo_t *cr = cairo_create(surface);

    cairo_set_source_rgb(cr, BG_R, BG_G, BG_B);
    cairo_paint(cr);

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    char clockbuf[32], datebuf[64], host[128] = "UNKNOWN";
    strftime(clockbuf, sizeof clockbuf, "%H:%M", &tm);
    strftime(datebuf, sizeof datebuf, "%A %d %B %Y", &tm);
    gethostname(host, sizeof host - 1);

    int monitor_count = 0;
    XRRMonitorInfo *monitors =
        XRRGetMonitors(display, root, True, &monitor_count);

    if (monitors && monitor_count > 0) {
        for (int i = 0; i < monitor_count; ++i) {
            draw_monitor(
                cr,
                monitors[i].x,
                monitors[i].y,
                monitors[i].width,
                monitors[i].height,
                clockbuf,
                datebuf,
                host
            );
        }
        XRRFreeMonitors(monitors);
    } else {
        draw_monitor(
            cr, 0, 0, root_width, root_height,
            clockbuf, datebuf, host
        );
    }

    cairo_status_t status = cairo_surface_write_to_png(surface, path);
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    XCloseDisplay(display);

    if (status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "vfdlock: PNG render failed: %s\n",
                cairo_status_to_string(status));
        return 1;
    }

    return 0;
}

int main(void) {
    const char *runtime = getenv("XDG_RUNTIME_DIR");
    if (!runtime) runtime = "/tmp";

    char path[512];
    snprintf(path, sizeof path, "%s/vfdlock-%ld.png",
             runtime, (long)getuid());

    if (render(path) != 0) return 1;

    execlp("i3lock", "i3lock",
           "--nofork",
           "--image", path,
           "--color", "0a0a0f",
           "--ignore-empty-password",
           (char *)NULL);

    fprintf(stderr, "vfdlock: failed to execute i3lock: %s\n",
            strerror(errno));
    unlink(path);
    return 1;
}
