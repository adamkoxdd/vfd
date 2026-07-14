#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <vfd/theme.h>
#include <vfd/ui.h>
#include <vfd/widgets.h>
#include <X11/keysym.h>
#include <dirent.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_ITEMS 256
#define MAX_LABEL 160
#define MAX_COMMAND 1024

typedef enum { PAGE_ROOT, PAGE_APPEARANCE, PAGE_COMPONENTS, PAGE_KEYS, PAGE_ABOUT } Page;
typedef enum { ENTRY_ACTION, ENTRY_PAGE } EntryType;
typedef struct { char label[MAX_LABEL], detail[MAX_LABEL], command[MAX_COMMAND]; EntryType type; Page page; } Entry;
typedef struct {
    VfdUi *ui; VfdWindow *win; VfdTheme theme;
    VfdColor bg,fg,dim,accent,selection;
    int width,height; bool dirty; Page page;
    Entry entries[MAX_ITEMS]; size_t count;
    const char *labels[MAX_ITEMS]; VfdListView list;
} App;
static volatile sig_atomic_t running=1;
static void stop_signal(int s){(void)s;running=0;}
static const char *home(void){const char*h=getenv("HOME");return h?h:"";}
static void current_theme(char*out,size_t n){char r[128]="lain";if(vfd_ipc_request("GET theme",r,sizeof r)==0){r[strcspn(r,"\r\n")]=0;if(*r){snprintf(out,n,"%s",r);return;}}snprintf(out,n,"lain");}
static const char *theme_path(void){static char p[PATH_MAX],name[64];current_theme(name,sizeof name);snprintf(p,sizeof p,"%s/.config/vfd/themes/%s/theme.ini",home(),name);if(access(p,R_OK)!=0)snprintf(p,sizeof p,"%s/.config/vfd/themes/lain/theme.ini",home());return p;}
static const char *page_name(Page p){switch(p){case PAGE_APPEARANCE:return"APPEARANCE";case PAGE_COMPONENTS:return"COMPONENTS";case PAGE_KEYS:return"KEYBINDINGS";case PAGE_ABOUT:return"ABOUT";default:return"SETTINGS";}}
static void add(App*a,EntryType t,Page p,const char*l,const char*d,const char*c){if(a->count>=MAX_ITEMS)return;Entry*e=&a->entries[a->count++];snprintf(e->label,sizeof e->label,"%s",l);snprintf(e->detail,sizeof e->detail,"%s",d?d:"");snprintf(e->command,sizeof e->command,"%s",c?c:"");e->type=t;e->page=p;}
static void page(App*a,Page p,const char*l,const char*d){add(a,ENTRY_PAGE,p,l,d,NULL);}static void action(App*a,const char*l,const char*d,const char*c){add(a,ENTRY_ACTION,PAGE_ROOT,l,d,c);}
static void load_root(App*a){page(a,PAGE_APPEARANCE,"Appearance","Themes, compositor and wallpaper");page(a,PAGE_COMPONENTS,"Components","Daemon, bar, Files and Shell controls");page(a,PAGE_KEYS,"Keybindings","Open and edit i3 bindings");page(a,PAGE_ABOUT,"About","Version, daemon state and diagnostics");}
static void load_appearance(App*a){char root[PATH_MAX],cur[64];current_theme(cur,sizeof cur);snprintf(root,sizeof root,"%s/.config/vfd/themes",home());DIR*d=opendir(root);if(d){struct dirent*de;while((de=readdir(d))){if(de->d_name[0]=='.')continue;char f[PATH_MAX];snprintf(f,sizeof f,"%s/%s/theme.ini",root,de->d_name);if(access(f,R_OK))continue;char detail[128],cmd[MAX_COMMAND];snprintf(detail,sizeof detail,"%s%s",de->d_name,!strcmp(cur,de->d_name)?"  [CURRENT]":"");snprintf(cmd,sizeof cmd,"vfdctl theme '%s'; pkill -HUP vfdbar 2>/dev/null || true",de->d_name);action(a,de->d_name,detail,cmd);}closedir(d);}action(a,"Edit Picom","Open ~/.config/picom/picom.conf","vfdterm nvim ~/.config/picom/picom.conf");action(a,"Edit Wallpaper","Open i3 wallpaper configuration","vfdterm nvim ~/.config/i3/config");}
static void load_components(App*a){action(a,"Restart Daemon","Restart vfdd","pkill -x vfdd; sleep 0.2; vfdd");action(a,"Restart Bar","Restart vfdbar","pkill -x vfdbar; sleep 0.2; vfdbar");action(a,"Open Files","Launch the VFD file manager","vfdfm ~");action(a,"Open Shell","Launch the VFD command shell","vfdshell");action(a,"Open Project","Browse ~/Projects/vfd","vfdfm ~/Projects/vfd");action(a,"Rebuild VFD","Run make and install in a terminal","vfdterm sh -lc 'cd ~/Projects/vfd && make clean && make && make install; echo; read -r -p \"Press Enter...\"'");}
static void load_keys(App*a){action(a,"Edit i3 Config","Open ~/.config/i3/config","vfdterm nvim ~/.config/i3/config");action(a,"Reload i3","Apply configuration changes","i3-msg reload");action(a,"Restart i3","Restart the window manager in place","i3-msg restart");action(a,"Show Bindings","Inspect configured binds","vfdterm sh -lc 'grep -n \"bindsym\" ~/.config/i3/config | less'");}
static void load_about(App*a){char status[256]="vfdd unavailable";if(vfd_ipc_request("STATUS",status,sizeof status)==0)status[strcspn(status,"\r\n")]=0;action(a,"VFD Desktop 0.6","Native desktop runtime","");action(a,"Daemon",status,"vfdterm sh -lc 'vfdctl status; echo; vfdctl snapshot; echo; read -r -p \"Press Enter...\"'");action(a,"Repository","~/Projects/vfd","vfdfm ~/Projects/vfd");action(a,"Diagnostics","Show daemon and process state","vfdterm sh -lc 'echo VFD DIAGNOSTICS; echo; vfdctl status; vfdctl snapshot; echo; pgrep -a vfdd; pgrep -a vfdbar; pgrep -a vfdfm; echo; read -r -p \"Press Enter...\"'");}
static void load(App*a){a->count=0;switch(a->page){case PAGE_APPEARANCE:load_appearance(a);break;case PAGE_COMPONENTS:load_components(a);break;case PAGE_KEYS:load_keys(a);break;case PAGE_ABOUT:load_about(a);break;default:load_root(a);}for(size_t i=0;i<a->count;i++)a->labels[i]=a->entries[i].label;vfd_list_set_items(&a->list,a->labels,a->count);vfd_list_first(&a->list);a->dirty=true;}
static void set_page(App*a,Page p){a->page=p;load(a);}static void back(App*a){if(a->page==PAGE_ROOT)running=0;else set_page(a,PAGE_ROOT);}
static void launch(const char*c){if(!c||!*c)return;pid_t p=fork();if(!p){setsid();execl("/bin/sh","sh","-lc",c,(char*)0);_exit(127);}}
static void activate(App*a){if(!a->count)return;size_t i=vfd_list_selected(&a->list);if(i>=a->count)return;Entry*e=&a->entries[i];if(e->type==ENTRY_PAGE)set_page(a,e->page);else if(e->command[0]){launch(e->command);running=0;}}
static int colors(App*a){return vfd_color_parse(a->win,a->theme.background,&a->bg)||vfd_color_parse(a->win,a->theme.foreground,&a->fg)||vfd_color_parse(a->win,a->theme.dim,&a->dim)||vfd_color_parse(a->win,a->theme.accent,&a->accent)||vfd_color_parse(a->win,a->theme.selection,&a->selection);}
static void render(App*a){vfd_begin_frame(a->win,a->bg);vfd_draw_rect(a->win,0,0,a->width,1,a->accent);vfd_label_draw(a->win,16,27,page_name(a->page),a->accent);vfd_draw_rect(a->win,12,39,a->width-24,1,a->selection);VfdRect b={12,51,a->width-24,a->height-87};vfd_list_draw(&a->list,a->win,b,17,a->fg,a->accent,a->selection);size_t rows=b.height/a->list.row_height;for(size_t r=0;r<rows&&a->list.scroll+r<a->count;r++){size_t i=a->list.scroll+r;int y=b.y+(int)r*a->list.row_height+17;vfd_draw_text(a->win,a->width-54,y,a->entries[i].type==ENTRY_PAGE?">":"SET",a->dim);}VfdStatusBar s={a->page==PAGE_ROOT?"CONTROL CENTER":"ESC BACK",a->page==PAGE_ROOT?"ENTER OPENS":"ENTER APPLIES","VFD 0.6",{0,a->height-24,a->width,24}};vfd_status_draw(&s,a->win,a->height-7,a->selection,a->dim,a->accent);vfd_end_frame(a->win);a->dirty=false;}
int main(void){signal(SIGINT,stop_signal);signal(SIGTERM,stop_signal);App a={.width=680,.height=470,.dirty=true,.page=PAGE_ROOT};vfd_theme_load(theme_path(),&a.theme);a.ui=vfd_ui_open();if(!a.ui){fprintf(stderr,"Settings: cannot open display\n");return 1;}int x,y,w,h;vfd_ui_monitor_geometry(a.ui,0,&x,&y,&w,&h);VfdWindowConfig c={x+(w-a.width)/2,y+(h-a.height)/3,a.width,a.height,"VFD Settings","vfdsettings",a.theme.font,false,0};a.win=vfd_window_create(a.ui,&c);if(!a.win||colors(&a)){fprintf(stderr,"Settings: window initialization failed\n");return 1;}vfd_list_init(&a.list,29);load(&a);while(running){struct pollfd p={vfd_ui_fd(a.ui),POLLIN,0};poll(&p,1,100);VfdEvent e;while(vfd_poll_event(a.ui,&e)){if(e.window!=a.win)continue;if(e.type==VFD_EVENT_EXPOSE)a.dirty=true;else if(e.type==VFD_EVENT_RESIZE){a.width=e.width;a.height=e.height;a.dirty=true;}else if(e.type==VFD_EVENT_CLOSE)running=0;else if(e.type==VFD_EVENT_KEY){if(e.keysym==XK_Escape||e.keysym==XK_BackSpace)back(&a);else if(e.keysym==XK_Return||e.keysym==XK_KP_Enter)activate(&a);else if(e.keysym==XK_Down||e.keysym==XK_j){vfd_list_move(&a.list,1);a.dirty=true;}else if(e.keysym==XK_Up||e.keysym==XK_k){vfd_list_move(&a.list,-1);a.dirty=true;}else if(e.keysym==XK_Page_Down){vfd_list_move(&a.list,8);a.dirty=true;}else if(e.keysym==XK_Page_Up){vfd_list_move(&a.list,-8);a.dirty=true;}}else if(e.type==VFD_EVENT_BUTTON&&e.button==1){VfdRect b={12,51,a.width-24,a.height-87};int i=vfd_list_index_at(&a.list,b,e.x,e.y);if(i>=0){vfd_list_select(&a.list,(size_t)i);activate(&a);}}}if(a.dirty)render(&a);}vfd_window_destroy(a.win);vfd_ui_close(a.ui);return 0;}
