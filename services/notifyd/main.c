#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <vfd/theme.h>
#include <vfd/ui.h>
#include <dbus/dbus.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/keysym.h>

#define MAX_NOTIFICATIONS 5
#define MAX_ACTIONS 8
#define WIDTH 390
#define HEIGHT 136
#define GAP 10
#define FADE_MS 450
#define FRAME_MS 20

typedef struct { char key[64]; char label[96]; } Action;

typedef struct {
    VfdWindow *win;
    unsigned id;
    char app[96],summary[256],body[512];
    Action actions[MAX_ACTIONS];
    int action_count,urgency,group_count;
    long created_ms,duration_ms,expires_ms,fade_started_ms;
    bool dirty,closing;
    unsigned close_reason;
} Notice;

typedef struct {
    VfdUi *ui; VfdTheme theme;
    VfdColor bg,fg,dim,accent,selection,alert;
    Notice notices[MAX_NOTIFICATIONS]; int count; unsigned next_id;
    DBusConnection *bus;
} App;

static volatile sig_atomic_t running=1;
static void stop_signal(int s){(void)s;running=0;}
static long now_ms(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1000L+t.tv_nsec/1000000L;}
static const char *theme_path(void){static char p[1024];snprintf(p,sizeof p,"%s/.config/vfd/themes/lain/theme.ini",getenv("HOME"));return p;}
static void clean(char*s){for(;*s;s++)if(*s=='\n'||*s=='\r'||*s=='\t')*s=' ';}
static double clamp01(double x){return x<0?0:x>1?1:x;}

static void emit_closed(App*a,unsigned id,unsigned reason){DBusMessage*m=dbus_message_new_signal("/org/freedesktop/Notifications","org.freedesktop.Notifications","NotificationClosed");dbus_uint32_t i=id,r=reason;if(m){dbus_message_append_args(m,DBUS_TYPE_UINT32,&i,DBUS_TYPE_UINT32,&r,DBUS_TYPE_INVALID);dbus_connection_send(a->bus,m,NULL);dbus_message_unref(m);}}
static void emit_action(App*a,unsigned id,const char*key){DBusMessage*m=dbus_message_new_signal("/org/freedesktop/Notifications","org.freedesktop.Notifications","ActionInvoked");dbus_uint32_t i=id;if(m){dbus_message_append_args(m,DBUS_TYPE_UINT32,&i,DBUS_TYPE_STRING,&key,DBUS_TYPE_INVALID);dbus_connection_send(a->bus,m,NULL);dbus_message_unref(m);}}
static void store_event(Notice*n){char q[1200],res[128];snprintf(q,sizeof q,"EVENT ADD\t%s\t%s\t%s",n->app,n->summary,n->body);vfd_ipc_request(q,res,sizeof res);}
static void position_all(App*a){int mx=0,my=0,mw=0,mh=0;vfd_ui_monitor_geometry(a->ui,0,&mx,&my,&mw,&mh);for(int i=0;i<a->count;i++)if(a->notices[i].win)vfd_window_move(a->notices[i].win,mx+mw-WIDTH-18,my+42+i*(HEIGHT+GAP));}
static void remove_at(App*a,int idx,unsigned reason){if(idx<0||idx>=a->count)return;unsigned id=a->notices[idx].id;if(a->notices[idx].win)vfd_window_destroy(a->notices[idx].win);memmove(&a->notices[idx],&a->notices[idx+1],sizeof(Notice)*(size_t)(a->count-idx-1));a->count--;emit_closed(a,id,reason);position_all(a);}
static void begin_close(Notice*n,unsigned reason){if(!n->closing){n->closing=true;n->close_reason=reason;n->fade_started_ms=now_ms();n->dirty=true;}}
static int find_id(App*a,unsigned id){for(int i=0;i<a->count;i++)if(a->notices[i].id==id)return i;return-1;}
static int find_duplicate(App*a,const char*app,const char*summary){for(int i=a->count-1;i>=0;i--){Notice*n=&a->notices[i];if(!n->closing&&!strcmp(n->app,app)&&!strcmp(n->summary,summary))return i;}return-1;}
static void parse_hints(DBusMessageIter*dict,int*urgency){*urgency=1;if(dbus_message_iter_get_arg_type(dict)!=DBUS_TYPE_ARRAY)return;DBusMessageIter array;dbus_message_iter_recurse(dict,&array);while(dbus_message_iter_get_arg_type(&array)==DBUS_TYPE_DICT_ENTRY){DBusMessageIter entry;dbus_message_iter_recurse(&array,&entry);const char*key="";dbus_message_iter_get_basic(&entry,&key);dbus_message_iter_next(&entry);if(!strcmp(key,"urgency")&&dbus_message_iter_get_arg_type(&entry)==DBUS_TYPE_VARIANT){DBusMessageIter v;dbus_message_iter_recurse(&entry,&v);unsigned char u=1;if(dbus_message_iter_get_arg_type(&v)==DBUS_TYPE_BYTE)dbus_message_iter_get_basic(&v,&u);*urgency=u;}dbus_message_iter_next(&array);}}
static void parse_actions(DBusMessageIter*iter,Notice*n){n->action_count=0;if(dbus_message_iter_get_arg_type(iter)!=DBUS_TYPE_ARRAY)return;DBusMessageIter arr;dbus_message_iter_recurse(iter,&arr);while(dbus_message_iter_get_arg_type(&arr)==DBUS_TYPE_STRING&&n->action_count<MAX_ACTIONS){const char*key="",*label="";dbus_message_iter_get_basic(&arr,&key);if(!dbus_message_iter_next(&arr))break;dbus_message_iter_get_basic(&arr,&label);dbus_message_iter_next(&arr);snprintf(n->actions[n->action_count].key,sizeof n->actions[n->action_count].key,"%s",key);snprintf(n->actions[n->action_count].label,sizeof n->actions[n->action_count].label,"%s",label);n->action_count++;}}

static void render(App*a,Notice*n,long now){
    VfdColor stripe=n->urgency>=2?a->alert:a->accent;
    vfd_begin_frame(n->win,a->bg);vfd_draw_rect(n->win,0,0,WIDTH,2,stripe);
    char appline[150];if(n->group_count>1)snprintf(appline,sizeof appline,"%s  //  x%d",n->app,n->group_count);else snprintf(appline,sizeof appline,"%s",n->app);
    vfd_draw_text(n->win,14,24,appline,stripe);vfd_draw_text(n->win,14,50,n->summary,a->fg);
    if(n->body[0]){char body[90];snprintf(body,sizeof body,"%.82s",n->body);vfd_draw_text(n->win,14,76,body,a->dim);}
    vfd_draw_rect(n->win,0,102,WIDTH,34,a->selection);
    if(n->action_count>0){char action[128];snprintf(action,sizeof action,"ENTER: %s",n->actions[0].label);vfd_draw_text(n->win,14,125,action,a->accent);}else vfd_draw_text(n->win,14,125,"CLICK TO DISMISS",a->dim);
    char t[32];time_t wall=time(NULL);strftime(t,sizeof t,"%H:%M:%S",localtime(&wall));vfd_draw_text(n->win,WIDTH-82,125,t,stripe);
    if(n->duration_ms>0&&!n->closing){double left=clamp01((double)(n->expires_ms-now)/(double)n->duration_ms);int bar=(int)((WIDTH-2)*left);vfd_draw_rect(n->win,1,HEIGHT-3,bar,2,stripe);}
    vfd_end_frame(n->win);n->dirty=false;
}

static int add_notice(App*a,unsigned replaces,const char*app,const char*summary,const char*body,int urgency,int timeout,Notice*parsed){
    char ca[96],cs[256];snprintf(ca,sizeof ca,"%s",app&&*app?app:"SYSTEM");snprintf(cs,sizeof cs,"%s",summary&&*summary?summary:"Notification");clean(ca);clean(cs);
    int idx=replaces?find_id(a,replaces):-1;if(idx<0)idx=find_duplicate(a,ca,cs);
    bool grouped=idx>=0&&!replaces;
    if(idx<0){if(a->count==MAX_NOTIFICATIONS)begin_close(&a->notices[0],2);if(a->count==MAX_NOTIFICATIONS)remove_at(a,0,2);idx=a->count++;memset(&a->notices[idx],0,sizeof a->notices[idx]);a->notices[idx].id=++a->next_id;a->notices[idx].group_count=1;}
    Notice*n=&a->notices[idx];if(grouped)n->group_count++;else if(!n->group_count)n->group_count=1;
    if(!n->win){int mx=0,my=0,mw=0,mh=0;vfd_ui_monitor_geometry(a->ui,0,&mx,&my,&mw,&mh);VfdWindowConfig c={mx+mw-WIDTH-18,my+42+idx*(HEIGHT+GAP),WIDTH,HEIGHT,"VFD Event","vfdnotify",a->theme.font,false,0};n->win=vfd_window_create(a->ui,&c);vfd_window_set_opacity(n->win,1.0);}
    snprintf(n->app,sizeof n->app,"%s",ca);snprintf(n->summary,sizeof n->summary,"%s",cs);snprintf(n->body,sizeof n->body,"%s",body?body:"");clean(n->body);n->urgency=urgency;n->action_count=parsed->action_count;memcpy(n->actions,parsed->actions,sizeof n->actions);
    if(timeout<0)timeout=urgency>=2?0:(urgency==0?3000:6000);n->created_ms=now_ms();n->duration_ms=timeout;n->expires_ms=timeout==0?0:n->created_ms+timeout;n->closing=false;n->fade_started_ms=0;n->dirty=true;store_event(n);position_all(a);return idx;
}

static DBusHandlerResult handle(DBusConnection*c,DBusMessage*m,void*data){
    App*a=data;const char*member=dbus_message_get_member(m);DBusMessage*r=NULL;if(!member)return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if(!strcmp(member,"GetCapabilities")){r=dbus_message_new_method_return(m);DBusMessageIter it,arr;dbus_message_iter_init_append(r,&it);dbus_message_iter_open_container(&it,DBUS_TYPE_ARRAY,"s",&arr);const char*caps[]={"body","actions","persistence"};for(size_t i=0;i<3;i++)dbus_message_iter_append_basic(&arr,DBUS_TYPE_STRING,&caps[i]);dbus_message_iter_close_container(&it,&arr);}
    else if(!strcmp(member,"GetServerInformation")){r=dbus_message_new_method_return(m);const char*name="VFD Notify",*vendor="VFD Desktop",*version="1.0.0",*spec="1.2";dbus_message_append_args(r,DBUS_TYPE_STRING,&name,DBUS_TYPE_STRING,&vendor,DBUS_TYPE_STRING,&version,DBUS_TYPE_STRING,&spec,DBUS_TYPE_INVALID);}
    else if(!strcmp(member,"CloseNotification")){dbus_uint32_t id=0;dbus_message_get_args(m,NULL,DBUS_TYPE_UINT32,&id,DBUS_TYPE_INVALID);int idx=find_id(a,id);if(idx>=0)begin_close(&a->notices[idx],3);r=dbus_message_new_method_return(m);}
    else if(!strcmp(member,"Notify")){DBusMessageIter it;if(!dbus_message_iter_init(m,&it))return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;const char*app="SYSTEM",*icon="",*summary="Notification",*body="";dbus_uint32_t replaces=0;dbus_int32_t timeout=-1;Notice parsed={0};int urgency=1;dbus_message_iter_get_basic(&it,&app);dbus_message_iter_next(&it);dbus_message_iter_get_basic(&it,&replaces);dbus_message_iter_next(&it);dbus_message_iter_get_basic(&it,&icon);dbus_message_iter_next(&it);dbus_message_iter_get_basic(&it,&summary);dbus_message_iter_next(&it);dbus_message_iter_get_basic(&it,&body);dbus_message_iter_next(&it);parse_actions(&it,&parsed);dbus_message_iter_next(&it);parse_hints(&it,&urgency);dbus_message_iter_next(&it);if(dbus_message_iter_get_arg_type(&it)==DBUS_TYPE_INT32)dbus_message_iter_get_basic(&it,&timeout);int idx=add_notice(a,replaces,app,summary,body,urgency,timeout,&parsed);r=dbus_message_new_method_return(m);dbus_uint32_t id=a->notices[idx].id;dbus_message_append_args(r,DBUS_TYPE_UINT32,&id,DBUS_TYPE_INVALID);}
    else return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;if(r){dbus_connection_send(c,r,NULL);dbus_message_unref(r);}return DBUS_HANDLER_RESULT_HANDLED;
}

int main(void){
    signal(SIGINT,stop_signal);signal(SIGTERM,stop_signal);App app={0};vfd_theme_load(theme_path(),&app.theme);app.ui=vfd_ui_open();if(!app.ui){fprintf(stderr,"vfdnotify: cannot open display\n");return 1;}
    DBusError e;dbus_error_init(&e);app.bus=dbus_bus_get(DBUS_BUS_SESSION,&e);if(!app.bus){fprintf(stderr,"vfdnotify: %s\n",e.message);return 1;}
    int ret=dbus_bus_request_name(app.bus,"org.freedesktop.Notifications",DBUS_NAME_FLAG_REPLACE_EXISTING,&e);if(ret!=DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER){fprintf(stderr,"vfdnotify: notification name unavailable\n");return 1;}
    DBusObjectPathVTable table={.message_function=handle};dbus_connection_register_object_path(app.bus,"/org/freedesktop/Notifications",&table,&app);
    while(running){
        dbus_connection_read_write_dispatch(app.bus,FRAME_MS);
        VfdEvent ev;while(vfd_poll_event(app.ui,&ev))for(int i=0;i<app.count;i++)if(ev.window==app.notices[i].win){Notice*n=&app.notices[i];if(ev.type==VFD_EVENT_EXPOSE)n->dirty=true;else if(ev.type==VFD_EVENT_CLOSE||ev.type==VFD_EVENT_BUTTON)begin_close(n,2);else if(ev.type==VFD_EVENT_KEY){if((ev.keysym==XK_Return||ev.keysym==XK_space)&&n->action_count){emit_action(&app,n->id,n->actions[0].key);begin_close(n,2);}else if(ev.keysym==XK_Escape)begin_close(n,2);}}
        long now=now_ms();
        for(int i=0;i<app.count;i++){Notice*n=&app.notices[i];if(!n->closing&&n->expires_ms&&now>=n->expires_ms)begin_close(n,1);if(n->closing){double opacity=1.0-(double)(now-n->fade_started_ms)/(double)FADE_MS;vfd_window_set_opacity(n->win,clamp01(opacity));if(opacity<=0.0){unsigned reason=n->close_reason;remove_at(&app,i,reason);i--;continue;}}n->dirty=true;}
        for(int i=0;i<app.count;i++){Notice*n=&app.notices[i];if(n->win&&app.bg.pixel==0){vfd_color_parse(n->win,app.theme.background,&app.bg);vfd_color_parse(n->win,app.theme.foreground,&app.fg);vfd_color_parse(n->win,app.theme.dim,&app.dim);vfd_color_parse(n->win,app.theme.accent,&app.accent);vfd_color_parse(n->win,app.theme.selection,&app.selection);vfd_color_parse(n->win,app.theme.alert,&app.alert);}if(n->dirty)render(&app,n,now);}
    }
    while(app.count)remove_at(&app,0,3);dbus_connection_unref(app.bus);vfd_ui_close(app.ui);return 0;
}
