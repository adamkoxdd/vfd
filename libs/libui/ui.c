#define _POSIX_C_SOURCE 200809L
#include <vfd/ui.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xinerama.h>
#include <stdlib.h>
#include <string.h>

struct VfdUi { Display *dpy; int screen; };
struct VfdWindow { VfdUi *ui; Window win; XftDraw *draw; XftFont *font; int width,height; struct VfdWindow *next; };
static VfdWindow *windows;
static VfdWindow *lookup(Window w){for(VfdWindow*p=windows;p;p=p->next)if(p->win==w)return p;return NULL;}
VfdUi *vfd_ui_open(void){VfdUi*u=calloc(1,sizeof*u);if(!u)return NULL;u->dpy=XOpenDisplay(NULL);if(!u->dpy){free(u);return NULL;}u->screen=DefaultScreen(u->dpy);return u;}
void vfd_ui_close(VfdUi*u){if(!u)return;XCloseDisplay(u->dpy);free(u);}
int vfd_ui_fd(VfdUi*u){return ConnectionNumber(u->dpy);}
int vfd_ui_monitor_count(VfdUi*u){int n=1;if(XineramaIsActive(u->dpy)){XineramaScreenInfo*s=XineramaQueryScreens(u->dpy,&n);if(s)XFree(s);}return n>0?n:1;}
int vfd_ui_monitor_geometry(VfdUi*u,int idx,int*x,int*y,int*w,int*h){int n=0;if(XineramaIsActive(u->dpy)){XineramaScreenInfo*s=XineramaQueryScreens(u->dpy,&n);if(s&&idx<n){*x=s[idx].x_org;*y=s[idx].y_org;*w=s[idx].width;*h=s[idx].height;XFree(s);return 0;}if(s)XFree(s);}if(idx)return-1;*x=0;*y=0;*w=DisplayWidth(u->dpy,u->screen);*h=DisplayHeight(u->dpy,u->screen);return 0;}
static void props(VfdWindow*w,const VfdWindowConfig*c){Display*d=w->ui->dpy;if(c->dock){Atom t=XInternAtom(d,"_NET_WM_WINDOW_TYPE",False),dock=XInternAtom(d,"_NET_WM_WINDOW_TYPE_DOCK",False);XChangeProperty(d,w->win,t,XA_ATOM,32,PropModeReplace,(unsigned char*)&dock,1);}XClassHint hint={(char*)(c->class_name?c->class_name:"vfd"),(char*)(c->class_name?c->class_name:"vfd")};XSetClassHint(d,w->win,&hint);if(c->top_strut>0){unsigned long p[12]={0},b[4]={0};p[2]=(unsigned long)c->top_strut;p[8]=(unsigned long)c->x;p[9]=(unsigned long)(c->x+c->width-1);b[2]=(unsigned long)c->top_strut;Atom pa=XInternAtom(d,"_NET_WM_STRUT_PARTIAL",False),ba=XInternAtom(d,"_NET_WM_STRUT",False);XChangeProperty(d,w->win,pa,XA_CARDINAL,32,PropModeReplace,(unsigned char*)p,12);XChangeProperty(d,w->win,ba,XA_CARDINAL,32,PropModeReplace,(unsigned char*)b,4);}}
VfdWindow *vfd_window_create(VfdUi*u,const VfdWindowConfig*c){VfdWindow*w=calloc(1,sizeof*w);if(!w)return NULL;w->ui=u;w->width=c->width;w->height=c->height;w->win=XCreateSimpleWindow(u->dpy,RootWindow(u->dpy,u->screen),c->x,c->y,c->width,c->height,0,0,0);XStoreName(u->dpy,w->win,c->title?c->title:"vfd");XSelectInput(u->dpy,w->win,ExposureMask|ButtonPressMask|KeyPressMask|StructureNotifyMask);props(w,c);w->font=XftFontOpenName(u->dpy,u->screen,c->font?c->font:"monospace:size=9");if(!w->font){XDestroyWindow(u->dpy,w->win);free(w);return NULL;}w->draw=XftDrawCreate(u->dpy,w->win,DefaultVisual(u->dpy,u->screen),DefaultColormap(u->dpy,u->screen));XMapRaised(u->dpy,w->win);w->next=windows;windows=w;return w;}
void vfd_window_move(VfdWindow*w,int x,int y){if(w)XMoveWindow(w->ui->dpy,w->win,x,y);}
void vfd_window_set_opacity(VfdWindow*w,double opacity){
    if(!w)return;
    if(opacity<0.0)opacity=0.0;
    if(opacity>1.0)opacity=1.0;
    unsigned long value=(unsigned long)(opacity*4294967295.0);
    Atom atom=XInternAtom(w->ui->dpy,"_NET_WM_WINDOW_OPACITY",False);
    XChangeProperty(w->ui->dpy,w->win,atom,XA_CARDINAL,32,PropModeReplace,(unsigned char*)&value,1);
    XFlush(w->ui->dpy);
}
void vfd_window_destroy(VfdWindow*w){if(!w)return;VfdWindow**p=&windows;while(*p&&*p!=w)p=&(*p)->next;if(*p)*p=w->next;XftDrawDestroy(w->draw);XftFontClose(w->ui->dpy,w->font);XDestroyWindow(w->ui->dpy,w->win);free(w);}
int vfd_color_parse(VfdWindow*w,const char*n,VfdColor*out){XftColor c;if(!XftColorAllocName(w->ui->dpy,DefaultVisual(w->ui->dpy,w->ui->screen),DefaultColormap(w->ui->dpy,w->ui->screen),n,&c))return-1;out->pixel=c.pixel;out->red=c.color.red;out->green=c.color.green;out->blue=c.color.blue;out->alpha=c.color.alpha;return 0;}
static XftColor xc(VfdColor c){XftColor x={.pixel=c.pixel,.color={c.red,c.green,c.blue,c.alpha}};return x;}
int vfd_text_width(VfdWindow*w,const char*t){XGlyphInfo e;XftTextExtentsUtf8(w->ui->dpy,w->font,(FcChar8*)t,(int)strlen(t),&e);return e.xOff;}
void vfd_begin_frame(VfdWindow*w,VfdColor bg){vfd_draw_rect(w,0,0,w->width,w->height,bg);}
void vfd_draw_rect(VfdWindow*w,int x,int y,int ww,int h,VfdColor c){XftColor z=xc(c);XftDrawRect(w->draw,&z,x,y,(unsigned)ww,(unsigned)h);}
void vfd_draw_text(VfdWindow*w,int x,int y,const char*t,VfdColor c){XftColor z=xc(c);XftDrawStringUtf8(w->draw,&z,w->font,x,y,(FcChar8*)t,(int)strlen(t));}
void vfd_end_frame(VfdWindow*w){XFlush(w->ui->dpy);}
int vfd_font_ascent(VfdWindow*w){return w->font->ascent;}int vfd_font_descent(VfdWindow*w){return w->font->descent;}
int vfd_poll_event(VfdUi*u,VfdEvent*out){if(!XPending(u->dpy))return 0;XEvent e;XNextEvent(u->dpy,&e);memset(out,0,sizeof*out);out->window=lookup(e.xany.window);if(e.type==Expose)out->type=VFD_EVENT_EXPOSE;else if(e.type==ButtonPress){out->type=VFD_EVENT_BUTTON;out->x=e.xbutton.x;out->y=e.xbutton.y;out->button=e.xbutton.button;}else if(e.type==KeyPress){out->type=VFD_EVENT_KEY;KeySym ks=0;int n=XLookupString(&e.xkey,out->text,(int)sizeof out->text-1,&ks,NULL);if(n<0)n=0;out->text[n]='\0';out->keysym=(unsigned long)ks;}else if(e.type==ConfigureNotify){out->type=VFD_EVENT_RESIZE;out->width=e.xconfigure.width;out->height=e.xconfigure.height;if(out->window){out->window->width=out->width;out->window->height=out->height;}}else if(e.type==DestroyNotify)out->type=VFD_EVENT_CLOSE;return out->type!=VFD_EVENT_NONE;}
