#define _POSIX_C_SOURCE 200809L
#include <vfd/config.h>
#include <vfd/util.h>
#include <stdio.h>
#include <string.h>
int vfd_config_get(const char *path,const char *section,const char *key,char *out,size_t n){FILE *f=fopen(path,"r");if(!f)return -1;char line[512],cur[128]="";int rc=-1;while(fgets(line,sizeof line,f)){char *s=vfd_trim(line);if(!*s||*s==';'||*s=='#')continue;if(*s=='['){char *r=strchr(s,']');if(r){*r='\0';snprintf(cur,sizeof cur,"%s",s+1);}continue;}char *eq=strchr(s,'=');if(!eq)continue;*eq='\0';char *k=vfd_trim(s),*v=vfd_trim(eq+1);if(!strcmp(cur,section)&&!strcmp(k,key)){snprintf(out,n,"%s",v);rc=0;break;}}fclose(f);return rc;}
