#include <vfd/theme.h>
#include <vfd/config.h>
#include <string.h>
static void get(const char*p,const char*k,char*out,size_t n,const char*d){if(vfd_config_get(p,"colors",k,out,n))strncpy(out,d,n-1);out[n-1]='\0';}
int vfd_theme_load(const char *p,VfdTheme*t){memset(t,0,sizeof *t);get(p,"background",t->background,sizeof t->background,"#0a0a0f");get(p,"foreground",t->foreground,sizeof t->foreground,"#b0b0c0");get(p,"dim",t->dim,sizeof t->dim,"#505060");get(p,"accent",t->accent,sizeof t->accent,"#9b7fd4");get(p,"selection",t->selection,sizeof t->selection,"#291f38");get(p,"alert",t->alert,sizeof t->alert,"#c47fb0");get(p,"font",t->font,sizeof t->font,"Terminus:pixelsize=10");return 0;}
