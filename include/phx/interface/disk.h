#ifndef PHX_INTERFACE_DISK_H
#define PHX_INTERFACE_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/device/file.h>
#include <phx/device/disk.h>

typedef struct PHX_Disk_Interface
{
    PHX_Bool (*getDevice)(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out);
    PHX_Bool (*formatDevice)(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out);

    const char* name;
    const char* type;
} PHX_Disk_Interface;

#ifdef __cplusplus
}
#endif

#endif
