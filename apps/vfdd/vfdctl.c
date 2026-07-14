#include <vfd/ipc.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){char q[512]="STATUS",r[4096];if(argc>1){if(!strcmp(argv[1],"get")&&argc>2)snprintf(q,sizeof q,"GET %s",argv[2]);else if(!strcmp(argv[1],"snapshot"))strcpy(q,"SNAPSHOT");else if(!strcmp(argv[1],"ping"))strcpy(q,"PING");else if(!strcmp(argv[1],"shutdown"))strcpy(q,"SHUTDOWN");else if(!strcmp(argv[1],"status"))strcpy(q,"STATUS");else if(!strcmp(argv[1],"theme")&&argc>2)snprintf(q,sizeof q,"THEME %s",argv[2]);else{q[0]=0;for(int i=1;i<argc;i++){if(i>1)strncat(q," ",sizeof q-strlen(q)-1);strncat(q,argv[i],sizeof q-strlen(q)-1);}}}if(vfd_ipc_request(q,r,sizeof r)){fprintf(stderr,"vfdd unavailable\n");return 1;}fputs(r,stdout);return !strncmp(r,"ERR",3);}
