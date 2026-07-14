#define _POSIX_C_SOURCE 200809L
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xinerama.h>
#include <gif_lib.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_MONITORS 16
#define MIN_FRAME_MS 20

typedef enum { SCALE_FILL, SCALE_FIT, SCALE_STRETCH, SCALE_CENTER } ScaleMode;

typedef struct {
    char file[PATH_MAX];
    ScaleMode scaling;
    int fps_cap;
    bool paused;
} Config;

typedef struct {
    int width, height, count;
    uint32_t **pixels;
    int *delay_ms;
} Animation;

typedef struct {
    Window win;
    GC gc;
    XImage *image;
    uint32_t *buffer;
    int x, y, width, height;
} Output;

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_requested = 0;
static void on_signal(int sig) {
    if (sig == SIGHUP) reload_requested = 1;
    else running = 0;
}

static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static void expand_home(char *out, size_t out_size, const char *in) {
    if (!in) { out[0] = 0; return; }
    if (in[0] == '~' && (in[1] == '/' || in[1] == 0)) {
        const char *home = getenv("HOME");
        snprintf(out, out_size, "%s%s", home ? home : "", in + 1);
    } else snprintf(out, out_size, "%s", in);
}

static char *trim(char *s) {
    while (*s == ' ' || *s == '\t') s++;
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r')) *--end = 0;
    return s;
}

static ScaleMode parse_scale(const char *s) {
    if (!strcasecmp(s, "fit")) return SCALE_FIT;
    if (!strcasecmp(s, "stretch")) return SCALE_STRETCH;
    if (!strcasecmp(s, "center")) return SCALE_CENTER;
    return SCALE_FILL;
}

static int load_config(Config *cfg, const char *override_file) {
    memset(cfg, 0, sizeof *cfg);
    cfg->scaling = SCALE_FILL;
    cfg->fps_cap = 30;
    const char *home = getenv("HOME");
    char path[PATH_MAX];
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) snprintf(path, sizeof path, "%s/vfd/wall.ini", xdg);
    else snprintf(path, sizeof path, "%s/.config/vfd/wall.ini", home ? home : ".");
    FILE *f = fopen(path, "r");
    if (f) {
        char line[PATH_MAX + 128];
        while (fgets(line, sizeof line, f)) {
            char *p = trim(line);
            if (!*p || *p == '#' || *p == ';' || *p == '[') continue;
            char *eq = strchr(p, '=');
            if (!eq) continue;
            *eq++ = 0;
            char *key = trim(p), *value = trim(eq);
            if (!strcmp(key, "file")) expand_home(cfg->file, sizeof cfg->file, value);
            else if (!strcmp(key, "scaling")) cfg->scaling = parse_scale(value);
            else if (!strcmp(key, "fps-cap")) cfg->fps_cap = atoi(value);
            else if (!strcmp(key, "paused")) cfg->paused = !strcasecmp(value, "true") || !strcmp(value, "1");
        }
        fclose(f);
    }
    if (override_file && *override_file) expand_home(cfg->file, sizeof cfg->file, override_file);
    if (cfg->fps_cap < 1) cfg->fps_cap = 1;
    if (cfg->fps_cap > 120) cfg->fps_cap = 120;
    return cfg->file[0] ? 0 : -1;
}

static int gif_transparency(const SavedImage *img) {
    for (int i = 0; i < img->ExtensionBlockCount; i++) {
        const ExtensionBlock *e = &img->ExtensionBlocks[i];
        if (e->Function == GRAPHICS_EXT_FUNC_CODE && e->ByteCount >= 4 && (e->Bytes[0] & 1))
            return (unsigned char)e->Bytes[3];
    }
    return -1;
}

static int gif_disposal(const SavedImage *img) {
    for (int i = 0; i < img->ExtensionBlockCount; i++) {
        const ExtensionBlock *e = &img->ExtensionBlocks[i];
        if (e->Function == GRAPHICS_EXT_FUNC_CODE && e->ByteCount >= 1)
            return ((unsigned char)e->Bytes[0] >> 2) & 7;
    }
    return 0;
}

static int gif_delay(const SavedImage *img) {
    for (int i = 0; i < img->ExtensionBlockCount; i++) {
        const ExtensionBlock *e = &img->ExtensionBlocks[i];
        if (e->Function == GRAPHICS_EXT_FUNC_CODE && e->ByteCount >= 3) {
            int cs = (unsigned char)e->Bytes[1] | ((unsigned char)e->Bytes[2] << 8);
            return cs > 0 ? cs * 10 : 100;
        }
    }
    return 100;
}

static uint32_t gif_color(const ColorMapObject *map, int idx) {
    if (!map || idx < 0 || idx >= map->ColorCount) return 0xff000000u;
    GifColorType c = map->Colors[idx];
    return 0xff000000u | ((uint32_t)c.Red << 16) | ((uint32_t)c.Green << 8) | c.Blue;
}

static void animation_free(Animation *a) {
    if (!a) return;
    for (int i = 0; i < a->count; i++) free(a->pixels[i]);
    free(a->pixels); free(a->delay_ms);
    memset(a, 0, sizeof *a);
}

static int animation_load(const char *path, Animation *a) {
    int err = 0;
    GifFileType *gif = DGifOpenFileName(path, &err);
    if (!gif) { fprintf(stderr, "vfdwall: cannot open %s (gif error %d)\n", path, err); return -1; }
    if (DGifSlurp(gif) == GIF_ERROR) {
        fprintf(stderr, "vfdwall: failed decoding %s: %s\n", path, GifErrorString(gif->Error));
        DGifCloseFile(gif, &err); return -1;
    }
    if (gif->ImageCount < 1 || gif->SWidth < 1 || gif->SHeight < 1) {
        fprintf(stderr, "vfdwall: empty GIF\n"); DGifCloseFile(gif, &err); return -1;
    }
    a->width = gif->SWidth; a->height = gif->SHeight; a->count = gif->ImageCount;
    a->pixels = calloc((size_t)a->count, sizeof *a->pixels);
    a->delay_ms = calloc((size_t)a->count, sizeof *a->delay_ms);
    uint32_t *canvas = malloc((size_t)a->width * a->height * sizeof *canvas);
    uint32_t *previous = malloc((size_t)a->width * a->height * sizeof *previous);
    if (!a->pixels || !a->delay_ms || !canvas || !previous) goto oom;
    uint32_t bg = gif_color(gif->SColorMap, gif->SBackGroundColor);
    for (int i = 0; i < a->width * a->height; i++) canvas[i] = bg;
    int prev_disposal = 0, prev_left = 0, prev_top = 0, prev_w = 0, prev_h = 0;
    for (int fi = 0; fi < a->count; fi++) {
        if (fi > 0) {
            if (prev_disposal == 2) {
                for (int y = 0; y < prev_h; y++) for (int x = 0; x < prev_w; x++) {
                    int dx = prev_left + x, dy = prev_top + y;
                    if (dx >= 0 && dx < a->width && dy >= 0 && dy < a->height) canvas[dy * a->width + dx] = bg;
                }
            } else if (prev_disposal == 3) memcpy(canvas, previous, (size_t)a->width * a->height * sizeof *canvas);
        }
        SavedImage *img = &gif->SavedImages[fi];
        int disposal = gif_disposal(img);
        if (disposal == 3) memcpy(previous, canvas, (size_t)a->width * a->height * sizeof *canvas);
        const ColorMapObject *map = img->ImageDesc.ColorMap ? img->ImageDesc.ColorMap : gif->SColorMap;
        int transparent = gif_transparency(img);
        int left = img->ImageDesc.Left, top = img->ImageDesc.Top;
        int w = img->ImageDesc.Width, h = img->ImageDesc.Height;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            int dx = left + x, dy = top + y;
            if (dx < 0 || dx >= a->width || dy < 0 || dy >= a->height) continue;
            int idx = img->RasterBits[y * w + x];
            if (idx != transparent) canvas[dy * a->width + dx] = gif_color(map, idx);
        }
        a->pixels[fi] = malloc((size_t)a->width * a->height * sizeof(uint32_t));
        if (!a->pixels[fi]) goto oom;
        memcpy(a->pixels[fi], canvas, (size_t)a->width * a->height * sizeof(uint32_t));
        a->delay_ms[fi] = gif_delay(img);
        prev_disposal = disposal; prev_left = left; prev_top = top; prev_w = w; prev_h = h;
    }
    free(canvas); free(previous); DGifCloseFile(gif, &err); return 0;
oom:
    fprintf(stderr, "vfdwall: out of memory\n");
    free(canvas); free(previous); DGifCloseFile(gif, &err); animation_free(a); return -1;
}

static unsigned long mask_scale(unsigned char value, unsigned long mask) {
    if (!mask) return 0;
    int shift = 0, bits = 0;
    while (((mask >> shift) & 1ul) == 0) shift++;
    unsigned long t = mask >> shift;
    while (t & 1ul) { bits++; t >>= 1; }
    unsigned long max = (1ul << bits) - 1ul;
    return (((unsigned long)value * max + 127ul) / 255ul << shift) & mask;
}

static unsigned long native_pixel(Visual *v, uint32_t rgba) {
    unsigned char r = (rgba >> 16) & 255, g = (rgba >> 8) & 255, b = rgba & 255;
    return mask_scale(r, v->red_mask) | mask_scale(g, v->green_mask) | mask_scale(b, v->blue_mask);
}

static void scale_frame(Output *o, Visual *visual, const Animation *a, int frame, ScaleMode mode) {
    double sx = (double)o->width / a->width, sy = (double)o->height / a->height;
    double scale_x = sx, scale_y = sy;
    int draw_w = o->width, draw_h = o->height, off_x = 0, off_y = 0;
    if (mode == SCALE_FILL || mode == SCALE_FIT) {
        double s = mode == SCALE_FILL ? (sx > sy ? sx : sy) : (sx < sy ? sx : sy);
        draw_w = (int)(a->width * s + 0.5); draw_h = (int)(a->height * s + 0.5);
        scale_x = scale_y = s; off_x = (o->width - draw_w) / 2; off_y = (o->height - draw_h) / 2;
    } else if (mode == SCALE_CENTER) {
        draw_w = a->width; draw_h = a->height; scale_x = scale_y = 1.0;
        off_x = (o->width - draw_w) / 2; off_y = (o->height - draw_h) / 2;
    }
    for (int y = 0; y < o->height; y++) for (int x = 0; x < o->width; x++) {
        int px = x - off_x, py = y - off_y;
        uint32_t color = 0xff0a0a0fu;
        if (px >= 0 && py >= 0 && px < draw_w && py < draw_h) {
            int src_x = (int)(px / scale_x), src_y = (int)(py / scale_y);
            if (src_x >= a->width) src_x = a->width - 1;
            if (src_y >= a->height) src_y = a->height - 1;
            color = a->pixels[frame][src_y * a->width + src_x];
        }
        XPutPixel(o->image, x, y, native_pixel(visual, color));
    }
}

static int create_outputs(Display *dpy, Output *outputs, int *output_count) {
    int screen = DefaultScreen(dpy), count = 0;
    XineramaScreenInfo *screens = NULL;
    if (XineramaIsActive(dpy)) screens = XineramaQueryScreens(dpy, &count);
    if (!screens || count < 1) count = 1;
    if (count > MAX_MONITORS) count = MAX_MONITORS;
    Visual *visual = DefaultVisual(dpy, screen);
    int depth = DefaultDepth(dpy, screen);
    Atom desktop = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Atom type_atom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
    Atom below = XInternAtom(dpy, "_NET_WM_STATE_BELOW", False);
    Atom state_atom = XInternAtom(dpy, "_NET_WM_STATE", False);
    for (int i = 0; i < count; i++) {
        Output *o = &outputs[i];
        o->x = screens ? screens[i].x_org : 0;
        o->y = screens ? screens[i].y_org : 0;
        o->width = screens ? screens[i].width : DisplayWidth(dpy, screen);
        o->height = screens ? screens[i].height : DisplayHeight(dpy, screen);
        XSetWindowAttributes attrs = {0}; attrs.override_redirect = True; attrs.background_pixel = BlackPixel(dpy, screen); attrs.event_mask = ExposureMask;
        o->win = XCreateWindow(dpy, RootWindow(dpy, screen), o->x, o->y, (unsigned)o->width, (unsigned)o->height, 0, depth, InputOutput, visual, CWOverrideRedirect | CWBackPixel | CWEventMask, &attrs);
        XStoreName(dpy, o->win, "VFD Wallpaper");
        XClassHint hint = { .res_name = "vfdwall", .res_class = "vfdwall" }; XSetClassHint(dpy, o->win, &hint);
        XChangeProperty(dpy, o->win, type_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&desktop, 1);
        XChangeProperty(dpy, o->win, state_atom, XA_ATOM, 32, PropModeReplace, (unsigned char *)&below, 1);
        o->gc = XCreateGC(dpy, o->win, 0, NULL);
        o->buffer = calloc((size_t)o->width * o->height, sizeof(uint32_t));
        o->image = XCreateImage(dpy, visual, (unsigned)depth, ZPixmap, 0, (char *)o->buffer, (unsigned)o->width, (unsigned)o->height, 32, 0);
        if (!o->image) return -1;
        XMapWindow(dpy, o->win); XLowerWindow(dpy, o->win);
    }
    if (screens) XFree(screens);
    *output_count = count; XFlush(dpy); return 0;
}

static void destroy_outputs(Display *dpy, Output *outputs, int count) {
    for (int i = 0; i < count; i++) {
        if (outputs[i].image) { outputs[i].image->data = (char *)outputs[i].buffer; XDestroyImage(outputs[i].image); }
        if (outputs[i].gc) XFreeGC(dpy, outputs[i].gc);
        if (outputs[i].win) XDestroyWindow(dpy, outputs[i].win);
    }
}

static void render(Display *dpy, Output *outputs, int count, const Animation *a, int frame, ScaleMode mode) {
    Visual *visual = DefaultVisual(dpy, DefaultScreen(dpy));
    for (int i = 0; i < count; i++) {
        scale_frame(&outputs[i], visual, a, frame, mode);
        XPutImage(dpy, outputs[i].win, outputs[i].gc, outputs[i].image, 0, 0, 0, 0, (unsigned)outputs[i].width, (unsigned)outputs[i].height);
        XLowerWindow(dpy, outputs[i].win);
    }
    XFlush(dpy);
}

static void usage(FILE *f) {
    fprintf(f, "usage: vfdwall [GIF]\n       vfdwall --help\n\nConfig: ~/.config/vfd/wall.ini\nSignals: SIGHUP reload, SIGTERM stop\n");
}

int main(int argc, char **argv) {
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) { usage(stdout); return 0; }
    signal(SIGINT, on_signal); signal(SIGTERM, on_signal); signal(SIGHUP, on_signal);
    Config cfg;
    if (load_config(&cfg, argc > 1 ? argv[1] : NULL) < 0) { fprintf(stderr, "vfdwall: set file=... in ~/.config/vfd/wall.ini or pass a GIF path\n"); return 1; }
    Animation anim = {0};
    if (animation_load(cfg.file, &anim) < 0) return 1;
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "vfdwall: cannot open DISPLAY\n"); animation_free(&anim); return 1; }
    Output outputs[MAX_MONITORS] = {0}; int output_count = 0;
    if (create_outputs(dpy, outputs, &output_count) < 0) { fprintf(stderr, "vfdwall: cannot create wallpaper windows\n"); XCloseDisplay(dpy); animation_free(&anim); return 1; }
    fprintf(stderr, "vfdwall: %s // %dx%d // %d frames // %d outputs\n", cfg.file, anim.width, anim.height, anim.count, output_count);
    int frame = 0; render(dpy, outputs, output_count, &anim, frame, cfg.scaling);
    uint64_t deadline = now_ms() + (uint64_t)anim.delay_ms[frame];
    while (running) {
        if (reload_requested) {
            reload_requested = 0;
            Config next; Animation next_anim = {0};
            if (load_config(&next, argc > 1 ? argv[1] : NULL) == 0 && animation_load(next.file, &next_anim) == 0) {
                animation_free(&anim); anim = next_anim; cfg = next; frame = 0; render(dpy, outputs, output_count, &anim, frame, cfg.scaling);
            }
            deadline = now_ms() + (uint64_t)anim.delay_ms[frame];
        }
        while (XPending(dpy)) { XEvent ev; XNextEvent(dpy, &ev); if (ev.type == Expose) render(dpy, outputs, output_count, &anim, frame, cfg.scaling); }
        uint64_t now = now_ms();
        int timeout = deadline > now ? (int)(deadline - now) : 0;
        struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
        poll(&pfd, 1, timeout);
        now = now_ms();
        if (!cfg.paused && now >= deadline) {
            frame = (frame + 1) % anim.count;
            render(dpy, outputs, output_count, &anim, frame, cfg.scaling);
            int min_delay = 1000 / cfg.fps_cap;
            int delay = anim.delay_ms[frame] < min_delay ? min_delay : anim.delay_ms[frame];
            if (delay < MIN_FRAME_MS) delay = MIN_FRAME_MS;
            deadline = now + (uint64_t)delay;
        }
    }
    destroy_outputs(dpy, outputs, output_count); XCloseDisplay(dpy); animation_free(&anim); return 0;
}
