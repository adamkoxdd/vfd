#define _POSIX_C_SOURCE 200809L
#include <vfd/util.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
char *vfd_trim(char *s){while(isspace((unsigned char)*s))s++;char *e=s+strlen(s);while(e>s&&isspace((unsigned char)e[-1]))*--e='\0';return s;}
int vfd_expand_home(const char *in,char *out,size_t n){const char *h=getenv("HOME");if(in[0]=='~'&&in[1]=='/'&&h)return snprintf(out,n,"%s/%s",h,in+2)<(int)n?0:-1;return snprintf(out,n,"%s",in)<(int)n?0:-1;}
