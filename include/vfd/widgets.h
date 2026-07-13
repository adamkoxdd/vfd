#ifndef VFD_WIDGETS_H
#define VFD_WIDGETS_H

#include <stdbool.h>
#include <stddef.h>
#include <vfd/ui.h>

typedef struct { int x, y, width, height; } VfdRect;
typedef enum { VFD_ALIGN_LEFT, VFD_ALIGN_CENTER, VFD_ALIGN_RIGHT } VfdAlign;

typedef struct {
    VfdRect bounds;
    int padding;
    int gap;
} VfdContainer;

typedef struct {
    const char **items;
    size_t count;
    size_t selected;
    size_t scroll;
    int row_height;
} VfdListView;

typedef struct {
    char text[128];
    bool active;
} VfdTextInput;

typedef struct {
    const char *label;
    VfdRect bounds;
    bool hovered;
    bool pressed;
    bool enabled;
} VfdButton;

typedef struct {
    size_t offset;
    size_t content_rows;
    size_t visible_rows;
} VfdScrollView;

typedef struct {
    const char *left;
    const char *center;
    const char *right;
    VfdRect bounds;
} VfdStatusBar;

bool vfd_rect_contains(VfdRect rect, int x, int y);
VfdRect vfd_container_content(const VfdContainer *container);
VfdRect vfd_container_row(const VfdContainer *container, size_t index, int height);

void vfd_label_draw(VfdWindow *win, int x, int baseline, const char *text, VfdColor color);
void vfd_label_draw_aligned(VfdWindow *win, VfdRect bounds, int baseline, const char *text, VfdAlign align, VfdColor color);

void vfd_list_init(VfdListView *list, int row_height);
void vfd_list_set_items(VfdListView *list, const char **items, size_t count);
void vfd_list_move(VfdListView *list, int delta);
void vfd_list_select(VfdListView *list, size_t index);
void vfd_list_first(VfdListView *list);
void vfd_list_last(VfdListView *list);
size_t vfd_list_selected(const VfdListView *list);
void vfd_list_ensure_visible(VfdListView *list, size_t visible_rows);
int vfd_list_index_at(const VfdListView *list, VfdRect bounds, int x, int y);
void vfd_list_draw(VfdListView *list, VfdWindow *win, VfdRect bounds, int baseline_offset, VfdColor fg, VfdColor accent, VfdColor selection);

void vfd_text_input_clear(VfdTextInput *input);
void vfd_text_input_backspace(VfdTextInput *input);
void vfd_text_input_append(VfdTextInput *input, const char *utf8);
void vfd_text_input_draw(VfdTextInput *input, VfdWindow *win, int x, int baseline, const char *prefix, VfdColor fg, VfdColor accent);

void vfd_button_init(VfdButton *button, const char *label, VfdRect bounds);
void vfd_button_pointer(VfdButton *button, int x, int y, bool pressed);
bool vfd_button_activate(const VfdButton *button, int x, int y);
void vfd_button_draw(const VfdButton *button, VfdWindow *win, int baseline, VfdColor fg, VfdColor accent, VfdColor background, VfdColor active_background);

void vfd_scroll_init(VfdScrollView *scroll);
void vfd_scroll_set(VfdScrollView *scroll, size_t content_rows, size_t visible_rows);
void vfd_scroll_by(VfdScrollView *scroll, int rows);
void vfd_scroll_to(VfdScrollView *scroll, size_t row);
size_t vfd_scroll_max(const VfdScrollView *scroll);

void vfd_status_draw(const VfdStatusBar *status, VfdWindow *win, int baseline, VfdColor background, VfdColor fg, VfdColor accent);

#endif
