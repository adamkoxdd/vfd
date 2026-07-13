#ifndef VFD_CONFIG_H
#define VFD_CONFIG_H
#include <stddef.h>
int vfd_config_get(const char *path,const char *section,const char *key,char *out,size_t n);
#endif
