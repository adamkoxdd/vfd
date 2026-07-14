#ifndef VFD_UI_H
#define VFD_UI_H
#include <stdbool.h>
#include <stddef.h>

typedef struct VfdUi VfdUi;
typedef struct VfdWindow VfdWindow;
typedef struct { unsigned long pixel; unsigned short red,green,blue,alpha; } VfdColor;
typedef enum { VFD_EVENT_NONE, VFD_EVENT_EXPOSE, VFD_EVENT_BUTTON, VFD_EVENT_KEY, VFD_EVENT_RESIZE, VFD_EVENT_CLOSE } VfdEventType;
typedef struct { VfdEventType type; VfdWindow *window; int x,y,button,width,height; unsigned long keysym; char text[16]; } VfdEvent;
typedef struct { int x,y,width,height; const char *title; const char *class_name; const char *font; bool dock; int top_strut; } VfdWindowConfig;

VfdUi *vfd_ui_open(void);
void vfd_ui_close(VfdUi *ui);
int vfd_ui_fd(VfdUi *ui);
int vfd_ui_monitor_count(VfdUi *ui);
int vfd_ui_monitor_geometry(VfdUi *ui,int index,int *x,int *y,int *w,int *h);
VfdWindow *vfd_window_create(VfdUi *ui,const VfdWindowConfig *cfg);
void vfd_window_destroy(VfdWindow *win);
void vfd_window_move(VfdWindow *win,int x,int y);
void vfd_window_set_opacity(VfdWindow *win,double opacity);
int vfd_color_parse(VfdWindow *win,const char *name,VfdColor *out);
int vfd_text_width(VfdWindow *win,const char *text);
void vfd_begin_frame(VfdWindow *win,VfdColor bg);
void vfd_draw_rect(VfdWindow *win,int x,int y,int w,int h,VfdColor color);
void vfd_draw_text(VfdWindow *win,int x,int baseline,const char *text,VfdColor color);
void vfd_end_frame(VfdWindow *win);
int vfd_font_ascent(VfdWindow *win);
int vfd_font_descent(VfdWindow *win);
int vfd_poll_event(VfdUi *ui,VfdEvent *event);
#endif
