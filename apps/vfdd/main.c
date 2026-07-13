#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static volatile sig_atomic_t run=1; static void stop(int s){(void)s;run=0;}
static int cpu(void){static unsigned long long pi,pt;FILE*f=fopen("/proc/stat","r");unsigned long long u,n,s,i,iw,irq,si,st;if(!f)return 0;fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",&u,&n,&s,&i,&iw,&irq,&si,&st);fclose(f);unsigned long long idle=i+iw,total=u+n+s+i+iw+irq+si+st,dt=total-pt,di=idle-pi;pt=total;pi=idle;return dt?(int)(100*(dt-di)/dt):0;}
static int mem(void){FILE*f=fopen("/proc/meminfo","r");char k[64];long v,total=0,avail=0;while(f&&fscanf(f,"%63s %ld kB",k,&v)==2){if(!strcmp(k,"MemTotal:"))total=v;else if(!strcmp(k,"MemAvailable:"))avail=v;}if(f)fclose(f);return total?(int)(100*(total-avail)/total):0;}
static int bat(char*st,size_t n){char path[128];FILE*f=NULL;int b=-1;for(int i=0;i<4&&!f;i++){snprintf(path,sizeof path,"/sys/class/power_supply/BAT%d/capacity",i);f=fopen(path,"r");}if(f){fscanf(f,"%d",&b);fclose(f);char *slash=strrchr(path,'/');if(slash)snprintf(slash+1,(size_t)(path+sizeof path-(slash+1)),"status");f=fopen(path,"r");}if(f){fgets(st,n,f);st[strcspn(st,"\n")]=0;fclose(f);}else snprintf(st,n,"Unknown");return b;}
static void reply(int fd,const char*q){char out[1024],bs[64],clock[64];time_t now=time(NULL);strftime(clock,sizeof clock,"%Y.%m.%d // %H:%M:%S",localtime(&now));int c=cpu(),m=mem(),b=bat(bs,sizeof bs);if(!strncmp(q,"GET ",4)){const char*k=q+4;if(!strcmp(k,"cpu"))snprintf(out,sizeof out,"%d\n",c);else if(!strcmp(k,"memory"))snprintf(out,sizeof out,"%d\n",m);else if(!strcmp(k,"battery"))snprintf(out,sizeof out,"%d\n",b);else if(!strcmp(k,"clock"))snprintf(out,sizeof out,"%s\n",clock);else if(!strcmp(k,"theme"))snprintf(out,sizeof out,"lain\n");else snprintf(out,sizeof out,"ERR unknown-key\n");}else if(!strcmp(q,"SNAPSHOT")){snprintf(out,sizeof out,"theme=lain\ncpu=%d\nmemory=%d\nbattery=%d\nbattery.status=%s\nclock=%s\n",c,m,b,bs,clock);}else if(!strcmp(q,"PING"))snprintf(out,sizeof out,"PONG\n");else if(!strcmp(q,"STATUS"))snprintf(out,sizeof out,"running=yes version=1.1.1 theme=lain\n");else if(!strcmp(q,"SHUTDOWN")){snprintf(out,sizeof out,"OK\n");run=0;}else snprintf(out,sizeof out,"ERR unknown-command\n");write(fd,out,strlen(out));}
int main(void){signal(SIGINT,stop);signal(SIGTERM,stop);char p[108];if(vfd_ipc_path(p,sizeof p)){fprintf(stderr,"XDG_RUNTIME_DIR missing\n");return 1;}unlink(p);int s=socket(AF_UNIX,SOCK_STREAM,0);struct sockaddr_un a={.sun_family=AF_UNIX};snprintf(a.sun_path,sizeof a.sun_path,"%s",p);if(bind(s,(void*)&a,sizeof a)||listen(s,8)){perror("vfdd");return 1;}while(run){struct pollfd f={s,POLLIN,0};if(poll(&f,1,500)>0){int c=accept(s,NULL,NULL);if(c>=0){char q[256];ssize_t n=read(c,q,sizeof q-1);if(n>0){q[n]=0;q[strcspn(q,"\r\n")]=0;reply(c,q);}close(c);}}}close(s);unlink(p);return 0;}
