#ifndef VFD_THEME_H
#define VFD_THEME_H
typedef struct {char background[16],foreground[16],dim[16],accent[16],selection[16],alert[16],font[128];} VfdTheme;
int vfd_theme_load(const char *path,VfdTheme *t);
#endif
