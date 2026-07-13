#include <vfd/widgets.h>

#include <stdio.h>
#include <string.h>

bool vfd_rect_contains(VfdRect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

VfdRect vfd_container_content(const VfdContainer *c) {
    VfdRect out = c ? c->bounds : (VfdRect){0};
    if (!c) return out;
    out.x += c->padding;
    out.y += c->padding;
    out.width -= c->padding * 2;
    out.height -= c->padding * 2;
    if (out.width < 0) out.width = 0;
    if (out.height < 0) out.height = 0;
    return out;
}

VfdRect vfd_container_row(const VfdContainer *c, size_t index, int height) {
    VfdRect content = vfd_container_content(c);
    content.y += (int)index * (height + (c ? c->gap : 0));
    content.height = height;
    return content;
}

void vfd_label_draw(VfdWindow *win, int x, int baseline, const char *text, VfdColor color) {
    vfd_draw_text(win, x, baseline, text ? text : "", color);
}

void vfd_label_draw_aligned(VfdWindow *win, VfdRect b, int baseline, const char *text, VfdAlign align, VfdColor color) {
    const char *safe = text ? text : "";
    int width = vfd_text_width(win, safe);
    int x = b.x;
    if (align == VFD_ALIGN_CENTER) x += (b.width - width) / 2;
    if (align == VFD_ALIGN_RIGHT) x += b.width - width;
    vfd_draw_text(win, x, b.y + baseline, safe, color);
}

void vfd_list_init(VfdListView *list, int row_height) {
    memset(list, 0, sizeof *list);
    list->row_height = row_height > 0 ? row_height : 20;
}

void vfd_list_set_items(VfdListView *list, const char **items, size_t count) {
    list->items = items;
    list->count = count;
    if (!count) list->selected = list->scroll = 0;
    else if (list->selected >= count) list->selected = count - 1;
}

void vfd_list_move(VfdListView *list, int delta) {
    if (!list->count) return;
    long next = (long)list->selected + delta;
    if (next < 0) next = 0;
    if ((size_t)next >= list->count) next = (long)list->count - 1;
    list->selected = (size_t)next;
}

void vfd_list_select(VfdListView *list, size_t index) {
    if (!list->count) return;
    list->selected = index < list->count ? index : list->count - 1;
}

void vfd_list_first(VfdListView *list) { list->selected = 0; }
void vfd_list_last(VfdListView *list) { if (list->count) list->selected = list->count - 1; }
size_t vfd_list_selected(const VfdListView *list) { return list->selected; }

void vfd_list_ensure_visible(VfdListView *list, size_t rows) {
    if (!rows) return;
    if (list->selected < list->scroll) list->scroll = list->selected;
    else if (list->selected >= list->scroll + rows) list->scroll = list->selected - rows + 1;
}

int vfd_list_index_at(const VfdListView *list, VfdRect b, int x, int y) {
    if (!vfd_rect_contains(b, x, y) || list->row_height <= 0) return -1;
    size_t row = (size_t)((y - b.y) / list->row_height);
    size_t index = list->scroll + row;
    return index < list->count ? (int)index : -1;
}

void vfd_list_draw(VfdListView *list, VfdWindow *win, VfdRect b, int base, VfdColor fg, VfdColor accent, VfdColor selection) {
    size_t rows = list->row_height > 0 ? (size_t)(b.height / list->row_height) : 0;
    vfd_list_ensure_visible(list, rows);
    for (size_t row = 0; row < rows && list->scroll + row < list->count; row++) {
        size_t index = list->scroll + row;
        int y = b.y + (int)row * list->row_height;
        bool selected = index == list->selected;
        if (selected) vfd_draw_rect(win, b.x, y, b.width, list->row_height, selection);
        vfd_draw_text(win, b.x + 8, y + base, selected ? ">" : " ", selected ? accent : fg);
        vfd_draw_text(win, b.x + 26, y + base, list->items[index] ? list->items[index] : "", selected ? accent : fg);
    }
}

void vfd_text_input_clear(VfdTextInput *input) { input->text[0] = '\0'; }
void vfd_text_input_backspace(VfdTextInput *input) {
    size_t n = strlen(input->text);
    if (n) input->text[n - 1] = '\0';
}
void vfd_text_input_append(VfdTextInput *input, const char *text) {
    if (!text || !*text) return;
    size_t used = strlen(input->text), add = strlen(text);
    if (used + add >= sizeof input->text) add = sizeof input->text - used - 1;
    memcpy(input->text + used, text, add);
    input->text[used + add] = '\0';
}
void vfd_text_input_draw(VfdTextInput *input, VfdWindow *win, int x, int baseline, const char *prefix, VfdColor fg, VfdColor accent) {
    char buffer[192];
    snprintf(buffer, sizeof buffer, "%s%s%s", prefix ? prefix : "", input->text, input->active ? "_" : "");
    vfd_draw_text(win, x, baseline, buffer, input->active ? accent : fg);
}

void vfd_button_init(VfdButton *button, const char *label, VfdRect bounds) {
    memset(button, 0, sizeof *button);
    button->label = label;
    button->bounds = bounds;
    button->enabled = true;
}
void vfd_button_pointer(VfdButton *button, int x, int y, bool pressed) {
    button->hovered = button->enabled && vfd_rect_contains(button->bounds, x, y);
    button->pressed = button->hovered && pressed;
}
bool vfd_button_activate(const VfdButton *button, int x, int y) {
    return button->enabled && vfd_rect_contains(button->bounds, x, y);
}
void vfd_button_draw(const VfdButton *button, VfdWindow *win, int baseline, VfdColor fg, VfdColor accent, VfdColor background, VfdColor active_background) {
    VfdColor bg = (button->pressed || button->hovered) ? active_background : background;
    VfdColor text = button->enabled ? ((button->pressed || button->hovered) ? accent : fg) : fg;
    vfd_draw_rect(win, button->bounds.x, button->bounds.y, button->bounds.width, button->bounds.height, bg);
    vfd_label_draw_aligned(win, button->bounds, baseline, button->label, VFD_ALIGN_CENTER, text);
}

void vfd_scroll_init(VfdScrollView *scroll) { memset(scroll, 0, sizeof *scroll); }
size_t vfd_scroll_max(const VfdScrollView *scroll) {
    return scroll->content_rows > scroll->visible_rows ? scroll->content_rows - scroll->visible_rows : 0;
}
void vfd_scroll_set(VfdScrollView *scroll, size_t content_rows, size_t visible_rows) {
    scroll->content_rows = content_rows;
    scroll->visible_rows = visible_rows;
    size_t max = vfd_scroll_max(scroll);
    if (scroll->offset > max) scroll->offset = max;
}
void vfd_scroll_by(VfdScrollView *scroll, int rows) {
    long next = (long)scroll->offset + rows;
    if (next < 0) next = 0;
    size_t max = vfd_scroll_max(scroll);
    if ((size_t)next > max) next = (long)max;
    scroll->offset = (size_t)next;
}
void vfd_scroll_to(VfdScrollView *scroll, size_t row) {
    size_t max = vfd_scroll_max(scroll);
    scroll->offset = row < max ? row : max;
}

void vfd_status_draw(const VfdStatusBar *status, VfdWindow *win, int baseline, VfdColor background, VfdColor fg, VfdColor accent) {
    if (!status) return;
    vfd_draw_rect(win, status->bounds.x, status->bounds.y, status->bounds.width, status->bounds.height, background);
    vfd_label_draw_aligned(win, status->bounds, baseline, status->left, VFD_ALIGN_LEFT, fg);
    vfd_label_draw_aligned(win, status->bounds, baseline, status->center, VFD_ALIGN_CENTER, accent);
    vfd_label_draw_aligned(win, status->bounds, baseline, status->right, VFD_ALIGN_RIGHT, fg);
}
