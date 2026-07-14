#define _POSIX_C_SOURCE 200809L
#include <vfd/ipc.h>
#include <vfd/theme.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>

static void value(const char *snapshot, const char *key, char *out, size_t n) {
    size_t k = strlen(key);
    const char *p = snapshot;
    while (p && *p) {
        if (!strncmp(p, key, k) && p[k] == '=') {
            p += k + 1;
            const char *e = strchr(p, '\n');
            size_t len = e ? (size_t)(e - p) : strlen(p);
            if (len >= n) len = n - 1;
            memcpy(out, p, len); out[len] = 0; return;
        }
        p = strchr(p, '\n'); if (p) p++;
    }
    snprintf(out, n, "?");
}

static const char *c(const char *hex, char *buf, size_t n) {
    unsigned r=155,g=127,b=212;
    if (hex && hex[0]=='#') sscanf(hex+1, "%02x%02x%02x", &r,&g,&b);
    snprintf(buf,n,"\033[38;2;%u;%u;%um",r,g,b); return buf;
}

int main(void) {
    char snapshot[2048]="", cpu[32], memory[32], battery[32], status[64], clock[128], theme_name[64];
    char theme_path[PATH_MAX], accent[40], dim[40];
    VfdTheme theme = {0}; struct utsname u; uname(&u);
    const char *home=getenv("HOME");
    snprintf(theme_path,sizeof theme_path,"%s/.config/vfd/themes/lain/theme.ini",home?home:"");
    if (vfd_theme_load(theme_path,&theme) != 0) { snprintf(theme.accent,sizeof theme.accent,"#9b7fd4"); snprintf(theme.dim,sizeof theme.dim,"#505060"); }
    if (vfd_ipc_request("SNAPSHOT",snapshot,sizeof snapshot) != 0) snprintf(snapshot,sizeof snapshot,"theme=offline\ncpu=?\nmemory=?\nbattery=?\nbattery.status=offline\nclock=?\n");
    value(snapshot,"cpu",cpu,sizeof cpu); value(snapshot,"memory",memory,sizeof memory);
    value(snapshot,"battery",battery,sizeof battery); value(snapshot,"battery.status",status,sizeof status);
    value(snapshot,"clock",clock,sizeof clock); value(snapshot,"theme",theme_name,sizeof theme_name);
    printf("%s", c(theme.accent,accent,sizeof accent));
    puts("        ██╗   ██╗███████╗██████╗ ");
    puts("        ██║   ██║██╔════╝██╔══██╗");
    puts("        ██║   ██║█████╗  ██║  ██║");
    puts("        ╚██╗ ██╔╝██╔══╝  ██║  ██║");
    puts("         ╚████╔╝ ██║     ██████╔╝");
    puts("          ╚═══╝  ╚═╝     ╚═════╝ ");
    printf("%s", c(theme.dim,dim,sizeof dim));
    printf("\n        HOST       %s\n", u.nodename);
    printf("        KERNEL     %s %s\n", u.sysname, u.release);
    printf("        SESSION    %s / %s\n", (getenv("XDG_CURRENT_DESKTOP") ? getenv("XDG_CURRENT_DESKTOP") : "i3"), (getenv("XDG_SESSION_TYPE") ? getenv("XDG_SESSION_TYPE") : "x11"));
    printf("        THEME      %s\n", theme_name);
    printf("        CPU        %s%%\n", cpu);
    printf("        MEMORY     %s%%\n", memory);
    printf("        BATTERY    %s%% // %s\n", battery, status);
    printf("        CLOCK      %s\n", clock);
    printf("\n%s        SYSTEM ONLINE // VFD-IPC %s\033[0m\n", c(theme.accent,accent,sizeof accent), strcmp(theme_name,"offline")?"CONNECTED":"OFFLINE");
    return 0;
}
