#ifndef VFDUI_H
#define VFDUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { double r, g, b, a; } VfdColor;
typedef struct { double x, y, width, height; } VfdRect;

typedef struct {
    VfdColor background;
    VfdColor panel;
    VfdColor phosphor;
    VfdColor glow;
    VfdColor dim;
    VfdColor danger;
    VfdColor success;
    double border_width;
    double corner_radius;
    double glow_strength;
    double scanline_alpha;
    int spacing;
    int font_size;
    int title_size;
    const char *font_family;
    bool scanlines;
} VfdTheme;

typedef struct VfdWindow VfdWindow;

typedef enum { VFD_ALIGN_LEFT, VFD_ALIGN_CENTER, VFD_ALIGN_RIGHT } VfdAlign;
typedef enum {
    VFD_KEY_NONE = 0, VFD_KEY_ESCAPE, VFD_KEY_ENTER, VFD_KEY_SPACE,
    VFD_KEY_LEFT, VFD_KEY_RIGHT, VFD_KEY_UP, VFD_KEY_DOWN,
    VFD_KEY_TAB, VFD_KEY_BACKSPACE, VFD_KEY_CHARACTER
} VfdKey;

typedef struct {
    bool quit, resized, mouse_moved, mouse_pressed, mouse_released;
    int mouse_x, mouse_y, width, height;
    VfdKey key;
    uint32_t codepoint;
    int scroll_y;
} VfdEvent;

typedef struct {
    char *buffer;
    size_t capacity, length, cursor;
    bool focused, password;
} VfdTextboxState;

typedef struct { int selected, hovered; double scroll; } VfdListState;
typedef struct { bool open; int selected, hovered; } VfdDropdownState;
typedef struct { const char *label; int depth; bool expanded, has_children; } VfdTreeRow;

/* Phase 3: animation/effects */
typedef enum {
    VFD_EASE_LINEAR,
    VFD_EASE_IN_QUAD,
    VFD_EASE_OUT_QUAD,
    VFD_EASE_IN_OUT_QUAD,
    VFD_EASE_OUT_CUBIC
} VfdEase;

typedef struct {
    double value;
    double start;
    double target;
    double elapsed;
    double duration;
    bool active;
    VfdEase ease;
} VfdAnim;

typedef struct {
    double opacity;
    double translate_x;
    double translate_y;
    double scale;
    double glow;
} VfdFx;

/* Phase 4: layout */
typedef enum { VFD_LAYOUT_HORIZONTAL, VFD_LAYOUT_VERTICAL, VFD_LAYOUT_GRID } VfdLayoutKind;
typedef enum {
    VFD_JUSTIFY_START,
    VFD_JUSTIFY_CENTER,
    VFD_JUSTIFY_END,
    VFD_JUSTIFY_SPACE_BETWEEN
} VfdJustify;
typedef enum { VFD_CROSS_START, VFD_CROSS_CENTER, VFD_CROSS_END, VFD_CROSS_STRETCH } VfdCross;

typedef struct {
    VfdLayoutKind kind;
    VfdRect bounds;
    double padding;
    double gap;
    int columns;
    VfdJustify justify;
    VfdCross cross;
    int index;
    int count;
} VfdLayout;

typedef enum {
    VFD_ANCHOR_TOP_LEFT,
    VFD_ANCHOR_TOP_CENTER,
    VFD_ANCHOR_TOP_RIGHT,
    VFD_ANCHOR_CENTER_LEFT,
    VFD_ANCHOR_CENTER,
    VFD_ANCHOR_CENTER_RIGHT,
    VFD_ANCHOR_BOTTOM_LEFT,
    VFD_ANCHOR_BOTTOM_CENTER,
    VFD_ANCHOR_BOTTOM_RIGHT
} VfdAnchor;

VfdTheme vfd_theme_default(void);
VfdColor vfd_color_hex(const char *hex);
VfdColor vfd_color_rgba(double r, double g, double b, double a);

VfdWindow *vfd_window_create(const char *title, int width, int height, const VfdTheme *theme);
void vfd_window_destroy(VfdWindow *window);
bool vfd_window_is_open(const VfdWindow *window);
void vfd_window_close(VfdWindow *window);
VfdEvent vfd_window_poll(VfdWindow *window);
void vfd_window_begin(VfdWindow *window);
void vfd_window_end(VfdWindow *window);
double vfd_window_time(const VfdWindow *window);
double vfd_window_delta(const VfdWindow *window);
int vfd_window_width(const VfdWindow *window);
int vfd_window_height(const VfdWindow *window);

void vfd_clear(VfdWindow *window);
void vfd_scanlines(VfdWindow *window);
void vfd_noise(VfdWindow *window, double amount);
void vfd_vignette(VfdWindow *window, double strength);
void vfd_crt_warmup(VfdWindow *window, double progress);
double vfd_pulse(double time_seconds, double speed, double minimum, double maximum);
bool vfd_blink(double time_seconds, double interval);

void vfd_anim_init(VfdAnim *anim, double value);
void vfd_anim_to(VfdAnim *anim, double target, double duration, VfdEase ease);
double vfd_anim_update(VfdAnim *anim, double delta);
double vfd_ease(VfdEase ease, double t);

void vfd_fx_push(VfdWindow *window, VfdFx fx);
void vfd_fx_pop(VfdWindow *window);

void vfd_panel(VfdWindow *window, VfdRect rect, bool filled);
void vfd_separator(VfdWindow *window, double x1, double y1, double x2, double y2);
void vfd_label(VfdWindow *window, const char *text, VfdRect rect, int size,
               bool bold, VfdAlign align, VfdColor color);
void vfd_title(VfdWindow *window, const char *text, VfdRect rect);
void vfd_progress(VfdWindow *window, VfdRect rect, double value, const char *label);
bool vfd_button(VfdWindow *window, VfdRect rect, const char *label, const VfdEvent *event);
void vfd_meter(VfdWindow *window, VfdRect rect, double value, int segments);
bool vfd_checkbox(VfdWindow *window, VfdRect rect, const char *label, bool *value, const VfdEvent *event);
bool vfd_switch(VfdWindow *window, VfdRect rect, const char *label, bool *value, const VfdEvent *event);
bool vfd_slider(VfdWindow *window, VfdRect rect, const char *label, double *value,
                double minimum, double maximum, const VfdEvent *event);
bool vfd_textbox(VfdWindow *window, VfdRect rect, const char *label,
                 VfdTextboxState *state, const VfdEvent *event);
int vfd_list(VfdWindow *window, VfdRect rect, const char *const *items,
             int item_count, int row_height, VfdListState *state, const VfdEvent *event);
int vfd_dropdown(VfdWindow *window, VfdRect rect, const char *label,
                 const char *const *items, int item_count,
                 VfdDropdownState *state, const VfdEvent *event);
int vfd_tree(VfdWindow *window, VfdRect rect, VfdTreeRow *rows, int row_count,
             int row_height, int *selected, const VfdEvent *event);
void vfd_scrollview_begin(VfdWindow *window, VfdRect rect, double scroll_y);
void vfd_scrollview_end(VfdWindow *window);

/* Layout helpers */
VfdRect vfd_inset(VfdRect rect, double all);
VfdRect vfd_insets(VfdRect rect, double left, double top, double right, double bottom);
VfdRect vfd_anchor(VfdRect parent, double width, double height, VfdAnchor anchor, double margin);
VfdLayout vfd_layout_vertical(VfdRect bounds, int count, double gap, double padding, VfdCross cross);
VfdLayout vfd_layout_horizontal(VfdRect bounds, int count, double gap, double padding, VfdCross cross);
VfdLayout vfd_layout_grid(VfdRect bounds, int count, int columns, double gap, double padding);
VfdRect vfd_layout_next(VfdLayout *layout);
VfdRect vfd_rect_split_left(VfdRect *remaining, double width, double gap);
VfdRect vfd_rect_split_top(VfdRect *remaining, double height, double gap);

#ifdef __cplusplus
}
#endif
#endif
