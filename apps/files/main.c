#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <vfd/theme.h>
#include <vfd/ui.h>
#include <vfd/widgets.h>
#include <X11/keysym.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILES 4096
#define MAX_NAME 256

typedef struct {char name[MAX_NAME];bool is_dir;off_t size;time_t mtime;} Entry;
typedef struct {
 VfdUi *ui; VfdWindow *win; int width,height; VfdTheme theme;
 VfdColor bg,fg,dim,accent,selection;
 char cwd[PATH_MAX]; Entry entries[MAX_FILES]; const char *labels[MAX_FILES]; size_t count;
 bool show_hidden,dirty; VfdListView list; VfdTextInput filter;
 int inotify_fd,watch_fd;
} App;
static volatile sig_atomic_t running=1;
static void stop(int s){(void)s;running=0;}
static const char *theme_path(void){static char p[PATH_MAX];const char*h=getenv("HOME");snprintf(p,sizeof p,"%s/.config/vfd/themes/lain/theme.ini",h?h:"");return p;}
static int cmp(const void*a,const void*b){const Entry*x=a,*y=b;if(x->is_dir!=y->is_dir)return y->is_dir-x->is_dir;return strcasecmp(x->name,y->name);}
static bool match(const App*a,const char*n){if(!a->filter.text[0])return true;char x[MAX_NAME],f[128];snprintf(x,sizeof x,"%s",n);snprintf(f,sizeof f,"%s",a->filter.text);for(char*p=x;*p;p++)if(*p>='A'&&*p<='Z')*p+=32;for(char*p=f;*p;p++)if(*p>='A'&&*p<='Z')*p+=32;return strstr(x,f)!=NULL;}
static void watch(App*a){if(a->watch_fd>=0)inotify_rm_watch(a->inotify_fd,a->watch_fd);a->watch_fd=inotify_add_watch(a->inotify_fd,a->cwd,IN_CREATE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO|IN_CLOSE_WRITE|IN_ATTRIB);}
static void load(App*a){DIR*d=opendir(a->cwd);if(!d)return;a->count=0;struct dirent*de;while((de=readdir(d))&&a->count<MAX_FILES){if(!strcmp(de->d_name,"."))continue;if(!a->show_hidden&&de->d_name[0]=='.'&&strcmp(de->d_name,".."))continue;if(!match(a,de->d_name))continue;char p[PATH_MAX];snprintf(p,sizeof p,"%s/%s",a->cwd,de->d_name);struct stat st;if(lstat(p,&st))continue;Entry*e=&a->entries[a->count++];snprintf(e->name,sizeof e->name,"%s",de->d_name);e->is_dir=S_ISDIR(st.st_mode);e->size=st.st_size;e->mtime=st.st_mtime;}closedir(d);qsort(a->entries,a->count,sizeof a->entries[0],cmp);for(size_t i=0;i<a->count;i++)a->labels[i]=a->entries[i].name;vfd_list_set_items(&a->list,a->labels,a->count);a->dirty=true;}
static void human(off_t n,char*out,size_t z){const char*u[]={"B","K","M","G","T"};double v=n;int i=0;while(v>=1024&&i<4){v/=1024;i++;}if(!i)snprintf(out,z,"%lldB",(long long)n);else snprintf(out,z,"%.1f%s",v,u[i]);}
static void open_file(const char*p){pid_t n=fork();if(!n){setsid();execlp("xdg-open","xdg-open",p,(char*)0);_exit(127);}}
static void parent(App*a){if(!strcmp(a->cwd,"/"))return;char*s=strrchr(a->cwd,'/');if(s==a->cwd)a->cwd[1]=0;else if(s)*s=0;vfd_list_first(&a->list);watch(a);load(a);}
static void open_selected(App*a){if(!a->count)return;size_t i=vfd_list_selected(&a->list);Entry*e=&a->entries[i];char p[PATH_MAX];snprintf(p,sizeof p,"%s/%s",a->cwd,e->name);if(e->is_dir){if(realpath(p,a->cwd)){vfd_list_first(&a->list);vfd_text_input_clear(&a->filter);watch(a);load(a);}}else open_file(p);}
static int colors(App*a){return vfd_color_parse(a->win,a->theme.background,&a->bg)||vfd_color_parse(a->win,a->theme.foreground,&a->fg)||vfd_color_parse(a->win,a->theme.dim,&a->dim)||vfd_color_parse(a->win,a->theme.accent,&a->accent)||vfd_color_parse(a->win,a->theme.selection,&a->selection);}
static void render(App*a){vfd_begin_frame(a->win,a->bg);vfd_draw_rect(a->win,0,0,a->width,1,a->accent);char title[PATH_MAX+8];snprintf(title,sizeof title,":: %s",a->cwd);vfd_label_draw(a->win,10,18,title,a->accent);VfdRect r={4,30,a->width-8,a->height-58};vfd_list_draw(&a->list,a->win,r,15,a->fg,a->accent,a->selection);for(size_t row=0;row<(size_t)(r.height/a->list.row_height)&&a->list.scroll+row<a->count;row++){size_t i=a->list.scroll+row;Entry*e=&a->entries[i];int y=r.y+(int)row*a->list.row_height+15;char sb[32]="DIR",tb[32];if(!e->is_dir)human(e->size,sb,sizeof sb);struct tm tmv;localtime_r(&e->mtime,&tmv);strftime(tb,sizeof tb,"%d %b  %H:%M",&tmv);vfd_draw_text(a->win,a->width-210,y,sb,a->dim);vfd_draw_text(a->win,a->width-125,y,tb,a->dim);}vfd_draw_rect(a->win,0,a->height-24,a->width,24,a->selection);char st[64];snprintf(st,sizeof st,"%zu ITEMS   %s",a->count,a->filter.active?"INSERT":"NORMAL");vfd_label_draw(a->win,10,a->height-7,st,a->dim);vfd_text_input_draw(&a->filter,a->win,170,a->height-7,"FILTER: ",a->dim,a->accent);vfd_label_draw(a->win,a->width-105,a->height-7,"FILES 0.2",a->accent);vfd_end_frame(a->win);a->dirty=false;}
static void key(App*a,const VfdEvent*e){unsigned long k=e->keysym;if(a->filter.active){if(k==XK_Escape||k==XK_Return){a->filter.active=false;a->dirty=true;return;}if(k==XK_BackSpace){vfd_text_input_backspace(&a->filter);load(a);return;}if(e->text[0]){vfd_text_input_append(&a->filter,e->text);load(a);}return;}if(k==XK_q||k==XK_Escape)running=0;else if(k==XK_j||k==XK_Down){vfd_list_move(&a->list,1);a->dirty=true;}else if(k==XK_k||k==XK_Up){vfd_list_move(&a->list,-1);a->dirty=true;}else if(k==XK_g){vfd_list_first(&a->list);a->dirty=true;}else if(k==XK_G){vfd_list_last(&a->list);a->dirty=true;}else if(k==XK_h||k==XK_Left)parent(a);else if(k==XK_l||k==XK_Right||k==XK_Return)open_selected(a);else if(k==XK_slash){a->filter.active=true;a->dirty=true;}else if(k==XK_period){a->show_hidden=!a->show_hidden;load(a);}else if(k==XK_r)load(a);}
int main(int argc,char**argv){signal(SIGINT,stop);signal(SIGTERM,stop);App a={.width=1000,.height=680,.watch_fd=-1,.inotify_fd=-1};const char*start=argc>1?argv[1]:getenv("HOME");if(!start||!realpath(start,a.cwd))snprintf(a.cwd,sizeof a.cwd,"/");vfd_theme_load(theme_path(),&a.theme);a.ui=vfd_ui_open();if(!a.ui){fprintf(stderr,"Files: cannot open display\n");return 1;}VfdWindowConfig c={100,100,a.width,a.height,"Files","vfdfm",a.theme.font,false,0};a.win=vfd_window_create(a.ui,&c);if(!a.win||colors(&a)){fprintf(stderr,"Files: window initialization failed\n");return 1;}vfd_list_init(&a.list,20);a.inotify_fd=inotify_init1(IN_NONBLOCK|IN_CLOEXEC);watch(&a);load(&a);while(running){struct pollfd fds[2]={{vfd_ui_fd(a.ui),POLLIN,0},{a.inotify_fd,POLLIN,0}};poll(fds,2,100);VfdEvent e;while(vfd_poll_event(a.ui,&e)){if(e.window!=a.win)continue;if(e.type==VFD_EVENT_EXPOSE)a.dirty=true;else if(e.type==VFD_EVENT_RESIZE){a.width=e.width;a.height=e.height;a.dirty=true;}else if(e.type==VFD_EVENT_KEY)key(&a,&e);else if(e.type==VFD_EVENT_CLOSE)running=0;}if(fds[1].revents&POLLIN){char b[4096];while(read(a.inotify_fd,b,sizeof b)>0){}load(&a);}if(a.dirty)render(&a);}if(a.watch_fd>=0)inotify_rm_watch(a.inotify_fd,a.watch_fd);if(a.inotify_fd>=0)close(a.inotify_fd);vfd_window_destroy(a.win);vfd_ui_close(a.ui);return 0;}
