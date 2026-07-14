#ifndef VFDUI_H
#define VFDUI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double r;
    double g;
    double b;
    double a;
} VfdColor;

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

typedef enum {
    VFD_ALIGN_LEFT,
    VFD_ALIGN_CENTER,
    VFD_ALIGN_RIGHT
} VfdAlign;

typedef enum {
    VFD_KEY_NONE = 0,
    VFD_KEY_ESCAPE,
    VFD_KEY_ENTER,
    VFD_KEY_SPACE,
    VFD_KEY_LEFT,
    VFD_KEY_RIGHT,
    VFD_KEY_UP,
    VFD_KEY_DOWN,
    VFD_KEY_TAB,
    VFD_KEY_BACKSPACE,
    VFD_KEY_CHARACTER
} VfdKey;

typedef struct {
    bool quit;
    bool resized;
    bool mouse_moved;
    bool mouse_pressed;
    bool mouse_released;
    int mouse_x;
    int mouse_y;
    int width;
    int height;
    VfdKey key;
    uint32_t codepoint;
    int scroll_y;
} VfdEvent;

typedef struct {
    double x;
    double y;
    double width;
    double height;
} VfdRect;

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
    size_t cursor;
    bool focused;
    bool password;
} VfdTextboxState;

typedef struct {
    int selected;
    int hovered;
    double scroll;
} VfdListState;

typedef struct {
    bool open;
    int selected;
    int hovered;
} VfdDropdownState;

typedef struct {
    const char *label;
    int depth;
    bool expanded;
    bool has_children;
} VfdTreeRow;

VfdTheme vfd_theme_default(void);

VfdWindow *vfd_window_create(
    const char *title,
    int width,
    int height,
    const VfdTheme *theme
);

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

void vfd_panel(VfdWindow *window, VfdRect rect, bool filled);
void vfd_separator(
    VfdWindow *window,
    double x1,
    double y1,
    double x2,
    double y2
);

void vfd_label(
    VfdWindow *window,
    const char *text,
    VfdRect rect,
    int size,
    bool bold,
    VfdAlign align,
    VfdColor color
);

void vfd_title(
    VfdWindow *window,
    const char *text,
    VfdRect rect
);

void vfd_progress(
    VfdWindow *window,
    VfdRect rect,
    double value,
    const char *label
);

bool vfd_button(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    const VfdEvent *event
);

void vfd_meter(
    VfdWindow *window,
    VfdRect rect,
    double value,
    int segments
);

bool vfd_checkbox(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    bool *value,
    const VfdEvent *event
);

bool vfd_switch(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    bool *value,
    const VfdEvent *event
);

bool vfd_slider(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    double *value,
    double minimum,
    double maximum,
    const VfdEvent *event
);

bool vfd_textbox(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    VfdTextboxState *state,
    const VfdEvent *event
);

int vfd_list(
    VfdWindow *window,
    VfdRect rect,
    const char *const *items,
    int item_count,
    int row_height,
    VfdListState *state,
    const VfdEvent *event
);

int vfd_dropdown(
    VfdWindow *window,
    VfdRect rect,
    const char *label,
    const char *const *items,
    int item_count,
    VfdDropdownState *state,
    const VfdEvent *event
);

int vfd_tree(
    VfdWindow *window,
    VfdRect rect,
    VfdTreeRow *rows,
    int row_count,
    int row_height,
    int *selected,
    const VfdEvent *event
);

void vfd_scrollview_begin(
    VfdWindow *window,
    VfdRect rect,
    double scroll_y
);

void vfd_scrollview_end(VfdWindow *window);

VfdColor vfd_color_hex(const char *hex);
VfdColor vfd_color_rgba(double r, double g, double b, double a);

#ifdef __cplusplus
}
#endif

#endif
