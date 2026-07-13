#ifndef VFD_UTIL_H
#define VFD_UTIL_H
#include <stddef.h>
char *vfd_trim(char *s);
int vfd_expand_home(const char *in,char *out,size_t n);
#endif
