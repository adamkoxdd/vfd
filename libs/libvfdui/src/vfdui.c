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
    int width, height, mouse_x, mouse_y;
    bool mouse_down;
    double start_time, last_time, delta;
    int clip_depth, fx_depth;
};

static double mono(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return ts.tv_sec+ts.tv_nsec/1e9; }
static double clampd(double v,double lo,double hi){ return v<lo?lo:(v>hi?hi:v); }
static void setc(cairo_t *cr,VfdColor c){ cairo_set_source_rgba(cr,c.r,c.g,c.b,c.a); }
static bool inside(VfdRect r,int x,int y){ return x>=r.x&&x<=r.x+r.width&&y>=r.y&&y<=r.y+r.height; }

static void rounded(cairo_t *cr,VfdRect r,double radius){
    double rr=fmin(radius,fmin(r.width,r.height)/2.0);
    cairo_new_sub_path(cr);
    cairo_arc(cr,r.x+r.width-rr,r.y+rr,rr,-G_PI/2,0);
    cairo_arc(cr,r.x+r.width-rr,r.y+r.height-rr,rr,0,G_PI/2);
    cairo_arc(cr,r.x+rr,r.y+r.height-rr,rr,G_PI/2,G_PI);
    cairo_arc(cr,r.x+rr,r.y+rr,rr,G_PI,3*G_PI/2);
    cairo_close_path(cr);
}
static void rebuild(VfdWindow *w){
    if(w->cr)cairo_destroy(w->cr);
    if(w->surface)cairo_surface_destroy(w->surface);
    w->surface=cairo_xlib_surface_create(w->display,w->xwindow,DefaultVisual(w->display,w->screen),w->width,w->height);
    w->cr=cairo_create(w->surface);
}

VfdColor vfd_color_rgba(double r,double g,double b,double a){ return (VfdColor){r,g,b,a}; }
VfdColor vfd_color_hex(const char *hex){
    unsigned r=0,g=0,b=0,a=255; if(!hex)return vfd_color_rgba(1,1,1,1); if(*hex=='#')hex++;
    if(strlen(hex)==6)sscanf(hex,"%02x%02x%02x",&r,&g,&b); else if(strlen(hex)==8)sscanf(hex,"%02x%02x%02x%02x",&r,&g,&b,&a);
    return vfd_color_rgba(r/255.0,g/255.0,b/255.0,a/255.0);
}
VfdTheme vfd_theme_default(void){
    return (VfdTheme){
        .background={.039,.039,.059,1},.panel={.059,.059,.102,1},
        .phosphor={.608,.498,.831,1},.glow={.725,.612,1,1},
        .dim={.310,.255,.427,1},.danger={.92,.35,.48,1},
        .success={.40,.82,.62,1},.border_width=2,.corner_radius=8,
        .glow_strength=.18,.scanline_alpha=.025,.spacing=12,
        .font_size=14,.title_size=19,.font_family="monospace",.scanlines=true
    };
}

VfdWindow *vfd_window_create(const char *title,int width,int height,const VfdTheme *theme){
    VfdWindow *w=calloc(1,sizeof(*w)); if(!w)return NULL;
    w->display=XOpenDisplay(NULL); if(!w->display){free(w);return NULL;}
    w->screen=DefaultScreen(w->display); w->width=width; w->height=height;
    w->theme=theme?*theme:vfd_theme_default(); w->open=true; w->start_time=w->last_time=mono();
    w->xwindow=XCreateSimpleWindow(w->display,RootWindow(w->display,w->screen),0,0,width,height,0,
        BlackPixel(w->display,w->screen),BlackPixel(w->display,w->screen));
    XStoreName(w->display,w->xwindow,title?title:"VFD");
    XSelectInput(w->display,w->xwindow,ExposureMask|StructureNotifyMask|KeyPressMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask);
    w->wm_delete=XInternAtom(w->display,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(w->display,w->xwindow,&w->wm_delete,1);
    XMapWindow(w->display,w->xwindow); rebuild(w); return w;
}
void vfd_window_destroy(VfdWindow *w){
    if(!w)return; while(w->clip_depth-->0)cairo_restore(w->cr); while(w->fx_depth-->0)cairo_restore(w->cr);
    if(w->cr)cairo_destroy(w->cr); if(w->surface)cairo_surface_destroy(w->surface);
    if(w->display&&w->xwindow)XDestroyWindow(w->display,w->xwindow); if(w->display)XCloseDisplay(w->display); free(w);
}
bool vfd_window_is_open(const VfdWindow *w){return w&&w->open;}
void vfd_window_close(VfdWindow *w){if(w)w->open=false;}
static VfdKey keymap(KeySym s,uint32_t *cp){
    *cp=0; switch(s){
        case XK_Escape:return VFD_KEY_ESCAPE; case XK_Return:return VFD_KEY_ENTER; case XK_space:return VFD_KEY_SPACE;
        case XK_Left:return VFD_KEY_LEFT; case XK_Right:return VFD_KEY_RIGHT; case XK_Up:return VFD_KEY_UP; case XK_Down:return VFD_KEY_DOWN;
        case XK_Tab:return VFD_KEY_TAB; case XK_BackSpace:return VFD_KEY_BACKSPACE;
        default: if(s>=XK_space&&s<=XK_asciitilde){*cp=(uint32_t)s;return VFD_KEY_CHARACTER;} return VFD_KEY_NONE;
    }
}
VfdEvent vfd_window_poll(VfdWindow *w){
    VfdEvent e={0}; if(!w){e.quit=true;return e;} e.width=w->width;e.height=w->height;e.mouse_x=w->mouse_x;e.mouse_y=w->mouse_y;
    while(XPending(w->display)){ XEvent x; XNextEvent(w->display,&x);
        switch(x.type){
            case ClientMessage: if((Atom)x.xclient.data.l[0]==w->wm_delete){w->open=false;e.quit=true;} break;
            case ConfigureNotify: if(x.xconfigure.width!=w->width||x.xconfigure.height!=w->height){w->width=x.xconfigure.width;w->height=x.xconfigure.height;rebuild(w);e.resized=true;e.width=w->width;e.height=w->height;} break;
            case MotionNotify:w->mouse_x=x.xmotion.x;w->mouse_y=x.xmotion.y;e.mouse_moved=true;e.mouse_x=w->mouse_x;e.mouse_y=w->mouse_y;break;
            case ButtonPress:w->mouse_down=true;w->mouse_x=x.xbutton.x;w->mouse_y=x.xbutton.y;e.mouse_x=w->mouse_x;e.mouse_y=w->mouse_y;
                if(x.xbutton.button==Button4)e.scroll_y=1;else if(x.xbutton.button==Button5)e.scroll_y=-1;else e.mouse_pressed=true;break;
            case ButtonRelease:w->mouse_down=false;w->mouse_x=x.xbutton.x;w->mouse_y=x.xbutton.y;e.mouse_released=true;e.mouse_x=w->mouse_x;e.mouse_y=w->mouse_y;break;
            case KeyPress:{KeySym s=XLookupKeysym(&x.xkey,0);e.key=keymap(s,&e.codepoint);if(e.key==VFD_KEY_ESCAPE)e.quit=true;}break;
        }
    } return e;
}
void vfd_window_begin(VfdWindow *w){ if(!w)return; double n=mono();w->delta=n-w->last_time;w->last_time=n;vfd_clear(w);}
void vfd_window_end(VfdWindow *w){ if(!w)return;while(w->clip_depth>0)vfd_scrollview_end(w);while(w->fx_depth>0)vfd_fx_pop(w);if(w->theme.scanlines)vfd_scanlines(w);cairo_surface_flush(w->surface);XFlush(w->display);}
double vfd_window_time(const VfdWindow *w){return w?mono()-w->start_time:0;}
double vfd_window_delta(const VfdWindow *w){return w?w->delta:0;}
int vfd_window_width(const VfdWindow *w){return w?w->width:0;} int vfd_window_height(const VfdWindow *w){return w?w->height:0;}

void vfd_clear(VfdWindow *w){if(!w)return;setc(w->cr,w->theme.background);cairo_paint(w->cr);}
void vfd_scanlines(VfdWindow *w){if(!w)return;cairo_save(w->cr);cairo_set_source_rgba(w->cr,w->theme.phosphor.r,w->theme.phosphor.g,w->theme.phosphor.b,w->theme.scanline_alpha);for(int y=0;y<w->height;y+=5)cairo_rectangle(w->cr,0,y,w->width,1);cairo_fill(w->cr);cairo_restore(w->cr);}
void vfd_noise(VfdWindow *w,double amount){
    if(!w||amount<=0)return; cairo_save(w->cr); unsigned seed=(unsigned)(vfd_window_time(w)*1000);
    for(int i=0;i<(int)(w->width*w->height*amount/7000.0);++i){seed=1664525u*seed+1013904223u;double x=seed%w->width;seed=1664525u*seed+1013904223u;double y=seed%w->height;cairo_set_source_rgba(w->cr,1,1,1,.03);cairo_rectangle(w->cr,x,y,1,1);cairo_fill(w->cr);} cairo_restore(w->cr);
}
void vfd_vignette(VfdWindow *w,double strength){
    if(!w||strength<=0)return;cairo_save(w->cr);double cx=w->width/2.0,cy=w->height/2.0,r=hypot(cx,cy);
    cairo_pattern_t *p=cairo_pattern_create_radial(cx,cy,r*.15,cx,cy,r);cairo_pattern_add_color_stop_rgba(p,0,0,0,0,0);cairo_pattern_add_color_stop_rgba(p,1,0,0,0,clampd(strength,0,1));cairo_set_source(w->cr,p);cairo_paint(w->cr);cairo_pattern_destroy(p);cairo_restore(w->cr);
}
void vfd_crt_warmup(VfdWindow *w,double p){
    if(!w)return;p=clampd(p,0,1);if(p>=1)return;cairo_save(w->cr);double h=fmax(2,w->height*(1-p));double y=(w->height-h)/2;cairo_set_source_rgba(w->cr,0,0,0,1-p*.95);cairo_paint(w->cr);cairo_set_source_rgba(w->cr,w->theme.glow.r,w->theme.glow.g,w->theme.glow.b,.8*(1-p));cairo_rectangle(w->cr,0,y,w->width,h);cairo_fill(w->cr);cairo_restore(w->cr);
}
double vfd_pulse(double t,double speed,double minimum,double maximum){double u=(sin(t*speed*G_PI*2)+1)/2;return minimum+(maximum-minimum)*u;}
bool vfd_blink(double t,double interval){if(interval<=0)return true;return fmod(t,interval)<interval/2;}

double vfd_ease(VfdEase ease,double t){t=clampd(t,0,1);switch(ease){case VFD_EASE_IN_QUAD:return t*t;case VFD_EASE_OUT_QUAD:return 1-(1-t)*(1-t);case VFD_EASE_IN_OUT_QUAD:return t<.5?2*t*t:1-pow(-2*t+2,2)/2;case VFD_EASE_OUT_CUBIC:return 1-pow(1-t,3);default:return t;}}
void vfd_anim_init(VfdAnim *a,double value){if(!a)return;*a=(VfdAnim){.value=value,.start=value,.target=value};}
void vfd_anim_to(VfdAnim *a,double target,double duration,VfdEase ease){if(!a)return;a->start=a->value;a->target=target;a->elapsed=0;a->duration=fmax(.0001,duration);a->ease=ease;a->active=true;}
double vfd_anim_update(VfdAnim *a,double delta){if(!a)return 0;if(!a->active)return a->value;a->elapsed+=fmax(0,delta);double t=clampd(a->elapsed/a->duration,0,1);double u=vfd_ease(a->ease,t);a->value=a->start+(a->target-a->start)*u;if(t>=1){a->value=a->target;a->active=false;}return a->value;}

void vfd_fx_push(VfdWindow *w,VfdFx fx){if(!w)return;cairo_save(w->cr);w->fx_depth++;cairo_translate(w->cr,fx.translate_x,fx.translate_y);cairo_scale(w->cr,fx.scale<=0?1:fx.scale,fx.scale<=0?1:fx.scale);cairo_push_group(w->cr);(void)fx;}
void vfd_fx_pop(VfdWindow *w){if(!w||w->fx_depth<=0)return;cairo_pattern_t *p=cairo_pop_group(w->cr);cairo_set_source(w->cr,p);cairo_paint(w->cr);cairo_pattern_destroy(p);cairo_restore(w->cr);w->fx_depth--;}

void vfd_panel(VfdWindow *w,VfdRect r,bool filled){if(!w)return;cairo_save(w->cr);rounded(w->cr,r,w->theme.corner_radius);if(filled){setc(w->cr,w->theme.panel);cairo_fill_preserve(w->cr);}cairo_set_line_width(w->cr,w->theme.border_width);setc(w->cr,w->theme.dim);cairo_stroke(w->cr);cairo_restore(w->cr);}
void vfd_separator(VfdWindow *w,double x1,double y1,double x2,double y2){if(!w)return;cairo_save(w->cr);cairo_set_line_width(w->cr,1);setc(w->cr,w->theme.dim);cairo_move_to(w->cr,x1,y1);cairo_line_to(w->cr,x2,y2);cairo_stroke(w->cr);cairo_restore(w->cr);}
void vfd_label(VfdWindow *w,const char *text,VfdRect r,int size,bool bold,VfdAlign align,VfdColor color){
    if(!w||!text)return;PangoLayout *l=pango_cairo_create_layout(w->cr);PangoFontDescription *f=pango_font_description_new();pango_font_description_set_family(f,w->theme.font_family);pango_font_description_set_absolute_size(f,size*PANGO_SCALE);pango_font_description_set_weight(f,bold?PANGO_WEIGHT_BOLD:PANGO_WEIGHT_NORMAL);pango_layout_set_font_description(l,f);pango_layout_set_text(l,text,-1);pango_layout_set_width(l,r.width*PANGO_SCALE);pango_layout_set_alignment(l,align==VFD_ALIGN_CENTER?PANGO_ALIGN_CENTER:align==VFD_ALIGN_RIGHT?PANGO_ALIGN_RIGHT:PANGO_ALIGN_LEFT);cairo_save(w->cr);setc(w->cr,color);cairo_move_to(w->cr,r.x,r.y);pango_cairo_show_layout(w->cr,l);cairo_restore(w->cr);pango_font_description_free(f);g_object_unref(l);
}
void vfd_title(VfdWindow *w,const char *t,VfdRect r){if(w)vfd_label(w,t,r,w->theme.title_size,true,VFD_ALIGN_LEFT,w->theme.phosphor);}
void vfd_progress(VfdWindow *w,VfdRect r,double value,const char *label){if(!w)return;value=clampd(value,0,1);vfd_panel(w,r,true);VfdRect f={r.x+3,r.y+3,(r.width-6)*value,r.height-6};cairo_save(w->cr);rounded(w->cr,f,w->theme.corner_radius/2);setc(w->cr,w->theme.phosphor);cairo_fill(w->cr);cairo_restore(w->cr);if(label)vfd_label(w,label,r,w->theme.font_size,true,VFD_ALIGN_CENTER,w->theme.glow);}
bool vfd_button(VfdWindow *w,VfdRect r,const char *label,const VfdEvent *e){if(!w||!e)return false;bool h=inside(r,e->mouse_x,e->mouse_y),c=h&&e->mouse_released;cairo_save(w->cr);rounded(w->cr,r,w->theme.corner_radius);VfdColor fill=w->theme.panel;if(h){fill.r+=w->theme.glow_strength;fill.g+=w->theme.glow_strength*.8;fill.b+=w->theme.glow_strength;}setc(w->cr,fill);cairo_fill_preserve(w->cr);cairo_set_line_width(w->cr,w->theme.border_width);setc(w->cr,h?w->theme.phosphor:w->theme.dim);cairo_stroke(w->cr);cairo_restore(w->cr);vfd_label(w,label,(VfdRect){r.x,r.y+(r.height-w->theme.font_size)/2-2,r.width,r.height},w->theme.font_size,true,VFD_ALIGN_CENTER,h?w->theme.glow:w->theme.phosphor);return c;}
void vfd_meter(VfdWindow *w,VfdRect r,double v,int seg){if(!w||seg<=0)return;v=clampd(v,0,1);double gap=3,sw=(r.width-gap*(seg-1))/seg;int active=round(v*seg);for(int i=0;i<seg;i++){VfdRect s={r.x+i*(sw+gap),r.y,sw,r.height};cairo_save(w->cr);rounded(w->cr,s,2);setc(w->cr,i<active?w->theme.phosphor:w->theme.dim);cairo_fill(w->cr);cairo_restore(w->cr);}}
bool vfd_checkbox(VfdWindow *w,VfdRect r,const char *label,bool *value,const VfdEvent *e){if(!w||!value||!e)return false;bool h=inside(r,e->mouse_x,e->mouse_y),ch=h&&e->mouse_released;if(ch)*value=!*value;VfdRect b={r.x,r.y,r.height,r.height};vfd_panel(w,b,true);if(*value){cairo_save(w->cr);setc(w->cr,w->theme.phosphor);cairo_set_line_width(w->cr,3);cairo_move_to(w->cr,b.x+6,b.y+b.height*.55);cairo_line_to(w->cr,b.x+b.width*.42,b.y+b.height-7);cairo_line_to(w->cr,b.x+b.width-6,b.y+7);cairo_stroke(w->cr);cairo_restore(w->cr);}vfd_label(w,label,(VfdRect){r.x+r.height+10,r.y+2,r.width-r.height-10,r.height},w->theme.font_size,h,VFD_ALIGN_LEFT,h?w->theme.glow:w->theme.phosphor);return ch;}
bool vfd_switch(VfdWindow *w,VfdRect r,const char *label,bool *value,const VfdEvent *e){if(!w||!value||!e)return false;bool h=inside(r,e->mouse_x,e->mouse_y),ch=h&&e->mouse_released;if(ch)*value=!*value;double tw=r.height*1.9;VfdRect t={r.x,r.y,tw,r.height};cairo_save(w->cr);rounded(w->cr,t,r.height/2);setc(w->cr,*value?w->theme.dim:w->theme.panel);cairo_fill_preserve(w->cr);setc(w->cr,*value?w->theme.phosphor:w->theme.dim);cairo_set_line_width(w->cr,w->theme.border_width);cairo_stroke(w->cr);double k=r.height-8,kx=*value?t.x+t.width-k-4:t.x+4;cairo_arc(w->cr,kx+k/2,t.y+t.height/2,k/2,0,G_PI*2);setc(w->cr,*value?w->theme.glow:w->theme.dim);cairo_fill(w->cr);cairo_restore(w->cr);vfd_label(w,label,(VfdRect){r.x+tw+12,r.y+2,r.width-tw-12,r.height},w->theme.font_size,h,VFD_ALIGN_LEFT,h?w->theme.glow:w->theme.phosphor);return ch;}
bool vfd_slider(VfdWindow *w,VfdRect r,const char *label,double *value,double min,double max,const VfdEvent *e){if(!w||!value||!e||max<=min)return false;bool h=inside(r,e->mouse_x,e->mouse_y),ch=false;if((e->mouse_pressed||e->mouse_moved)&&h&&w->mouse_down){double t=(e->mouse_x-r.x)/r.width;*value=min+clampd(t,0,1)*(max-min);ch=true;}double t=clampd((*value-min)/(max-min),0,1);cairo_save(w->cr);cairo_set_line_width(w->cr,4);setc(w->cr,w->theme.dim);cairo_move_to(w->cr,r.x,r.y+r.height/2);cairo_line_to(w->cr,r.x+r.width,r.y+r.height/2);cairo_stroke(w->cr);setc(w->cr,w->theme.phosphor);cairo_move_to(w->cr,r.x,r.y+r.height/2);cairo_line_to(w->cr,r.x+r.width*t,r.y+r.height/2);cairo_stroke(w->cr);cairo_arc(w->cr,r.x+r.width*t,r.y+r.height/2,h?9:7,0,G_PI*2);setc(w->cr,h?w->theme.glow:w->theme.phosphor);cairo_fill(w->cr);cairo_restore(w->cr);if(label){char buf[128];snprintf(buf,sizeof buf,"%s // %.2f",label,*value);vfd_label(w,buf,(VfdRect){r.x,r.y-24,r.width,20},12,h,VFD_ALIGN_LEFT,h?w->theme.glow:w->theme.dim);}return ch;}
bool vfd_textbox(VfdWindow *w,VfdRect r,const char *label,VfdTextboxState *s,const VfdEvent *e){if(!w||!s||!s->buffer||!s->capacity||!e)return false;bool h=inside(r,e->mouse_x,e->mouse_y),submit=false;if(e->mouse_released)s->focused=h;if(s->focused){if(e->key==VFD_KEY_BACKSPACE&&s->cursor>0){memmove(s->buffer+s->cursor-1,s->buffer+s->cursor,s->length-s->cursor+1);s->cursor--;s->length--;}else if(e->key==VFD_KEY_LEFT&&s->cursor>0)s->cursor--;else if(e->key==VFD_KEY_RIGHT&&s->cursor<s->length)s->cursor++;else if(e->key==VFD_KEY_ENTER)submit=true;else if(e->key==VFD_KEY_CHARACTER&&s->length+1<s->capacity&&e->codepoint>=32&&e->codepoint<=126){memmove(s->buffer+s->cursor+1,s->buffer+s->cursor,s->length-s->cursor+1);s->buffer[s->cursor]=(char)e->codepoint;s->cursor++;s->length++;}}vfd_panel(w,r,true);if(label)vfd_label(w,label,(VfdRect){r.x+12,r.y+7,r.width-24,18},11,true,VFD_ALIGN_LEFT,s->focused?w->theme.glow:w->theme.dim);char *d=calloc(s->length+2,1);if(!d)return submit;if(s->password)memset(d,'*',s->length);else memcpy(d,s->buffer,s->length);if(s->focused&&vfd_blink(vfd_window_time(w),1)&&s->cursor<=s->length){memmove(d+s->cursor+1,d+s->cursor,s->length-s->cursor+1);d[s->cursor]='_';}vfd_label(w,d,(VfdRect){r.x+12,r.y+(label?25:10),r.width-24,r.height-12},w->theme.font_size,false,VFD_ALIGN_LEFT,w->theme.phosphor);free(d);return submit;}
void vfd_scrollview_begin(VfdWindow *w,VfdRect r,double sy){if(!w)return;cairo_save(w->cr);w->clip_depth++;cairo_rectangle(w->cr,r.x,r.y,r.width,r.height);cairo_clip(w->cr);cairo_translate(w->cr,0,-sy);}
void vfd_scrollview_end(VfdWindow *w){if(!w||w->clip_depth<=0)return;cairo_restore(w->cr);w->clip_depth--;}
int vfd_list(VfdWindow *w,VfdRect r,const char *const *items,int count,int rh,VfdListState *s,const VfdEvent *e){if(!w||!items||count<=0||!s||!e)return -1;if(inside(r,e->mouse_x,e->mouse_y)&&e->scroll_y)s->scroll-=e->scroll_y*rh*2;double content=(double)count*rh,max=fmax(0,content-r.height);s->scroll=clampd(s->scroll,0,max);s->hovered=-1;vfd_panel(w,r,true);vfd_scrollview_begin(w,r,s->scroll);int clicked=-1;for(int i=0;i<count;i++){VfdRect row={r.x+4,r.y+i*rh+4,r.width-8,rh-4};int vy=row.y-s->scroll;bool h=e->mouse_x>=row.x&&e->mouse_x<=row.x+row.width&&e->mouse_y>=vy&&e->mouse_y<=vy+row.height&&inside(r,e->mouse_x,e->mouse_y);if(h)s->hovered=i;if(h&&e->mouse_released){s->selected=i;clicked=i;}if(i==s->selected||h){cairo_save(w->cr);rounded(w->cr,row,4);setc(w->cr,vfd_color_rgba(w->theme.phosphor.r,w->theme.phosphor.g,w->theme.phosphor.b,i==s->selected?.18:.08));cairo_fill(w->cr);cairo_restore(w->cr);}vfd_label(w,items[i],(VfdRect){row.x+10,row.y+7,row.width-20,row.height},w->theme.font_size,i==s->selected,VFD_ALIGN_LEFT,i==s->selected||h?w->theme.glow:w->theme.phosphor);}vfd_scrollview_end(w);return clicked;}
int vfd_dropdown(VfdWindow *w,VfdRect r,const char *label,const char *const *items,int count,VfdDropdownState *s,const VfdEvent *e){if(!w||!items||count<=0||!s||!e)return -1;if(s->selected<0||s->selected>=count)s->selected=0;bool h=inside(r,e->mouse_x,e->mouse_y);if(h&&e->mouse_released)s->open=!s->open;vfd_panel(w,r,true);char buf[256];snprintf(buf,sizeof buf,"%s // %s %s",label?label:"SELECT",items[s->selected],s->open?"▲":"▼");vfd_label(w,buf,(VfdRect){r.x+12,r.y+10,r.width-24,r.height},w->theme.font_size,h,VFD_ALIGN_LEFT,h?w->theme.glow:w->theme.phosphor);int ch=-1;if(s->open){VfdRect m={r.x,r.y+r.height+6,r.width,count*34.0+8};vfd_panel(w,m,true);for(int i=0;i<count;i++){VfdRect row={m.x+4,m.y+4+i*34,m.width-8,30};bool rh=inside(row,e->mouse_x,e->mouse_y);if(rh&&e->mouse_released){s->selected=i;s->open=false;ch=i;}if(rh||i==s->selected){cairo_save(w->cr);rounded(w->cr,row,4);setc(w->cr,vfd_color_rgba(w->theme.phosphor.r,w->theme.phosphor.g,w->theme.phosphor.b,rh?.16:.1));cairo_fill(w->cr);cairo_restore(w->cr);}vfd_label(w,items[i],(VfdRect){row.x+10,row.y+6,row.width-20,row.height},w->theme.font_size,i==s->selected,VFD_ALIGN_LEFT,rh||i==s->selected?w->theme.glow:w->theme.phosphor);}}return ch;}
int vfd_tree(VfdWindow *w,VfdRect r,VfdTreeRow *rows,int count,int rh,int *sel,const VfdEvent *e){if(!w||!rows||count<=0||!sel||!e)return -1;vfd_panel(w,r,true);int clicked=-1,vi=0,hidden=-1;for(int i=0;i<count;i++){if(hidden>=0){if(rows[i].depth>hidden)continue;hidden=-1;}VfdRect row={r.x+4,r.y+4+vi*rh,r.width-8,rh-4};vi++;bool h=inside(row,e->mouse_x,e->mouse_y);if(h&&e->mouse_released){*sel=i;clicked=i;if(rows[i].has_children)rows[i].expanded=!rows[i].expanded;}if(*sel==i||h){cairo_save(w->cr);rounded(w->cr,row,4);setc(w->cr,vfd_color_rgba(w->theme.phosphor.r,w->theme.phosphor.g,w->theme.phosphor.b,*sel==i?.18:.08));cairo_fill(w->cr);cairo_restore(w->cr);}char buf[256];snprintf(buf,sizeof buf,"%s%s",rows[i].has_children?(rows[i].expanded?"▼ ":"▶ "):"  ",rows[i].label?rows[i].label:"");vfd_label(w,buf,(VfdRect){row.x+8+rows[i].depth*18,row.y+6,row.width-16-rows[i].depth*18,row.height},w->theme.font_size,*sel==i,VFD_ALIGN_LEFT,h||*sel==i?w->theme.glow:w->theme.phosphor);if(rows[i].has_children&&!rows[i].expanded)hidden=rows[i].depth;}return clicked;}

/* Layout */
VfdRect vfd_inset(VfdRect r,double a){return (VfdRect){r.x+a,r.y+a,fmax(0,r.width-2*a),fmax(0,r.height-2*a)};}
VfdRect vfd_insets(VfdRect r,double l,double t,double rr,double b){return (VfdRect){r.x+l,r.y+t,fmax(0,r.width-l-rr),fmax(0,r.height-t-b)};}
VfdRect vfd_anchor(VfdRect p,double w,double h,VfdAnchor a,double m){
    double x=p.x+m,y=p.y+m; if(a==VFD_ANCHOR_TOP_CENTER||a==VFD_ANCHOR_CENTER||a==VFD_ANCHOR_BOTTOM_CENTER)x=p.x+(p.width-w)/2;
    else if(a==VFD_ANCHOR_TOP_RIGHT||a==VFD_ANCHOR_CENTER_RIGHT||a==VFD_ANCHOR_BOTTOM_RIGHT)x=p.x+p.width-w-m;
    if(a==VFD_ANCHOR_CENTER_LEFT||a==VFD_ANCHOR_CENTER||a==VFD_ANCHOR_CENTER_RIGHT)y=p.y+(p.height-h)/2;
    else if(a==VFD_ANCHOR_BOTTOM_LEFT||a==VFD_ANCHOR_BOTTOM_CENTER||a==VFD_ANCHOR_BOTTOM_RIGHT)y=p.y+p.height-h-m;
    return (VfdRect){x,y,w,h};
}
static VfdLayout layout_base(VfdLayoutKind kind,VfdRect b,int count,double gap,double pad,VfdCross cross){return (VfdLayout){.kind=kind,.bounds=b,.padding=pad,.gap=gap,.columns=1,.justify=VFD_JUSTIFY_START,.cross=cross,.index=0,.count=count};}
VfdLayout vfd_layout_vertical(VfdRect b,int count,double gap,double pad,VfdCross cross){return layout_base(VFD_LAYOUT_VERTICAL,b,count,gap,pad,cross);}
VfdLayout vfd_layout_horizontal(VfdRect b,int count,double gap,double pad,VfdCross cross){return layout_base(VFD_LAYOUT_HORIZONTAL,b,count,gap,pad,cross);}
VfdLayout vfd_layout_grid(VfdRect b,int count,int columns,double gap,double pad){VfdLayout l=layout_base(VFD_LAYOUT_GRID,b,count,gap,pad,VFD_CROSS_STRETCH);l.columns=columns>0?columns:1;return l;}
VfdRect vfd_layout_next(VfdLayout *l){
    if(!l||l->index>=l->count)return (VfdRect){0};VfdRect in=vfd_inset(l->bounds,l->padding);int i=l->index++;
    if(l->kind==VFD_LAYOUT_VERTICAL){double h=(in.height-l->gap*(l->count-1))/l->count;return (VfdRect){in.x,in.y+i*(h+l->gap),in.width,h};}
    if(l->kind==VFD_LAYOUT_HORIZONTAL){double w=(in.width-l->gap*(l->count-1))/l->count;return (VfdRect){in.x+i*(w+l->gap),in.y,w,in.height};}
    int cols=l->columns,rows=(l->count+cols-1)/cols,row=i/cols,col=i%cols;double cw=(in.width-l->gap*(cols-1))/cols,ch=(in.height-l->gap*(rows-1))/rows;return (VfdRect){in.x+col*(cw+l->gap),in.y+row*(ch+l->gap),cw,ch};
}
VfdRect vfd_rect_split_left(VfdRect *r,double w,double gap){if(!r)return (VfdRect){0};w=fmin(w,r->width);VfdRect out={r->x,r->y,w,r->height};r->x+=w+gap;r->width=fmax(0,r->width-w-gap);return out;}
VfdRect vfd_rect_split_top(VfdRect *r,double h,double gap){if(!r)return (VfdRect){0};h=fmin(h,r->height);VfdRect out={r->x,r->y,r->width,h};r->y+=h+gap;r->height=fmax(0,r->height-h-gap);return out;}
