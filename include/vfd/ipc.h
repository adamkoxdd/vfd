#ifndef VFD_IPC_H
#define VFD_IPC_H
#include <stddef.h>
int vfd_ipc_path(char *out,size_t n);
int vfd_ipc_request(const char *request,char *response,size_t n);
#endif
