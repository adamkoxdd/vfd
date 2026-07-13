#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
int vfd_ipc_path(char*out,size_t n){
    const char*r=getenv("XDG_RUNTIME_DIR");
    int written;
    if(r && *r) written=snprintf(out,n,"%s/vfdd.sock",r);
    else written=snprintf(out,n,"/run/user/%lu/vfdd.sock",(unsigned long)getuid());
    return written>=0 && written<(int)n ? 0 : -1;
}
int vfd_ipc_request(const char *req,char *res,size_t n){char p[108];if(vfd_ipc_path(p,sizeof p))return -1;int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return -1;struct sockaddr_un a={.sun_family=AF_UNIX};snprintf(a.sun_path,sizeof a.sun_path,"%s",p);if(connect(fd,(void*)&a,sizeof a)){close(fd);return -1;}dprintf(fd,"%s\n",req);ssize_t r=read(fd,res,n-1);close(fd);if(r<0)return -1;res[r]='\0';return 0;}
