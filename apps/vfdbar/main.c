#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <vfd/theme.h>
#include <vfd/ui.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_BARS 16
#define MAX_HITS 16

typedef struct { int x,w; const char *cmd; } Hit;
typedef struct { VfdWindow *w; int width; Hit hits[MAX_HITS]; int nhit; VfdColor bg,fg,dim,accent,selection; } Bar;
typedef struct { char cpu[16], memory[16], battery[16], unread[16], clock[128]; int connected; } State;

static volatile sig_atomic_t running=1;
static void stop(int s){(void)s;running=0;}

static void state_unknown(State *s){
    snprintf(s->cpu,sizeof s->cpu,"?");
    snprintf(s->memory,sizeof s->memory,"?");
    snprintf(s->battery,sizeof s->battery,"?");
    snprintf(s->unread,sizeof s->unread,"0");
    snprintf(s->clock,sizeof s->clock,"--.--.---- // --:--:--");
    s->connected=0;
}

static void copy_value(char *dst,size_t n,const char *src){
    if(!src)return;
    snprintf(dst,n,"%s",src);
    dst[strcspn(dst,"\r\n")]=0;
}

static int fetch_state(State *s){
    char response[1024];
    if(vfd_ipc_request("SNAPSHOT",response,sizeof response)!=0){state_unknown(s);return -1;}
    State next=*s;
    char *save=NULL;
    for(char *line=strtok_r(response,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        char *eq=strchr(line,'=');
        if(!eq)continue;
        *eq++='\0';
        if(!strcmp(line,"cpu"))copy_value(next.cpu,sizeof next.cpu,eq);
        else if(!strcmp(line,"memory"))copy_value(next.memory,sizeof next.memory,eq);
        else if(!strcmp(line,"battery"))copy_value(next.battery,sizeof next.battery,eq);
        else if(!strcmp(line,"clock"))copy_value(next.clock,sizeof next.clock,eq);
        else if(!strcmp(line,"events.unread"))copy_value(next.unread,sizeof next.unread,eq);
    }
    next.connected=1;
    *s=next;
    return 0;
}

static void launch(const char*c){if(!c||!*c)return;pid_t p=fork();if(!p){setsid();execl("/bin/sh","sh","-c",c,(char*)0);_exit(127);}if(p>0)signal(SIGCHLD,SIG_IGN);}
static const char *theme_path(void){static char p[1024];const char*h=getenv("HOME");snprintf(p,sizeof p,"%s/.config/vfd/themes/lain/theme.ini",h?h:"");return p;}
static int load_colors(Bar*b,VfdTheme*t){return vfd_color_parse(b->w,t->background,&b->bg)||vfd_color_parse(b->w,t->foreground,&b->fg)||vfd_color_parse(b->w,t->dim,&b->dim)||vfd_color_parse(b->w,t->accent,&b->accent)||vfd_color_parse(b->w,t->selection,&b->selection);}
static int baseline(Bar*b,int h){return (h+vfd_font_ascent(b->w)-vfd_font_descent(b->w))/2;}
static int draw_item(Bar*b,int x,const char*text,const char*cmd,int h){int width=vfd_text_width(b->w,text);vfd_draw_text(b->w,x,baseline(b,h),text,b->accent);if(cmd&&b->nhit<MAX_HITS)b->hits[b->nhit++]=(Hit){x,width,cmd};return x+width+14;}

static void render(Bar*b,int h,const State *s){
    b->nhit=0;
    vfd_begin_frame(b->w,b->bg);
    vfd_draw_rect(b->w,0,h-1,b->width,1,b->accent);
    int x=10;
    x=draw_item(b,x,"FILES","$HOME/.local/bin/vfdfm",h);
    x=draw_item(b,x,"TERM","vfdterm",h);
    x=draw_item(b,x,"WEB","firefox",h);
    x=draw_item(b,x,"MUSIC","spotify",h);
    x=draw_item(b,x,"NVIM","vfdterm nvim",h);
    x=draw_item(b,x,"DISCORD","discord",h);
    if(atoi(s->unread)>0){char ev[32];snprintf(ev,sizeof ev,"EVENTS %s",s->unread);x=draw_item(b,x,ev,"vfdshell",h);}
    int cw=vfd_text_width(b->w,s->clock);
    vfd_draw_text(b->w,(b->width-cw)/2,baseline(b,h),s->clock,s->connected?b->accent:b->dim);
    char right[160];
    snprintf(right,sizeof right,"CPU %s%%  MEM %s%%  BAT %s%%",s->cpu,s->memory,s->battery);
    int rw=vfd_text_width(b->w,right);
    vfd_draw_text(b->w,b->width-rw-10,baseline(b,h),right,s->connected?b->fg:b->dim);
    vfd_end_frame(b->w);
}

int main(void){
    signal(SIGINT,stop);signal(SIGTERM,stop);
    VfdTheme theme;
    if(vfd_theme_load(theme_path(),&theme)){fprintf(stderr,"vfdbar: cannot load theme\n");return 1;}
    VfdUi*ui=vfd_ui_open();
    if(!ui){fprintf(stderr,"vfdbar: cannot open display\n");return 1;}
    Bar bars[MAX_BARS]={0};
    int n=vfd_ui_monitor_count(ui);if(n>MAX_BARS)n=MAX_BARS;
    const int h=24;
    for(int i=0;i<n;i++){
        int x,y,w,mh;vfd_ui_monitor_geometry(ui,i,&x,&y,&w,&mh);
        VfdWindowConfig c={x,y,w,h,"vfdbar","vfdbar","Terminus:style=Regular:pixelsize=10",true,h};
        bars[i].w=vfd_window_create(ui,&c);bars[i].width=w;
        if(!bars[i].w||load_colors(&bars[i],&theme)){fprintf(stderr,"vfdbar: window init failed\n");return 1;}
    }
    State state;state_unknown(&state);fetch_state(&state);
    for(int i=0;i<n;i++)render(&bars[i],h,&state);
    while(running){
        struct pollfd p={vfd_ui_fd(ui),POLLIN,0};poll(&p,1,1000);
        int changed=fetch_state(&state)==0;
        VfdEvent e;
        while(vfd_poll_event(ui,&e)){
            for(int i=0;i<n;i++)if(e.window==bars[i].w){
                if(e.type==VFD_EVENT_BUTTON){for(int j=0;j<bars[i].nhit;j++)if(e.x>=bars[i].hits[j].x&&e.x<bars[i].hits[j].x+bars[i].hits[j].w){launch(bars[i].hits[j].cmd);break;}}
                changed=1;
            }
        }
        if(changed)for(int i=0;i<n;i++)render(&bars[i],h,&state);
    }
    for(int i=0;i<n;i++)vfd_window_destroy(bars[i].w);
    vfd_ui_close(ui);return 0;
}
