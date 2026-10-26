#ifndef PHX_INTERFACE_FILESYSTEM_H
#define PHX_INTERFACE_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/device/disk.h>
#include <phx/device/filesystem.h>

typedef struct PHX_Filesystem_Interface
{
    PHX_Result (*openFilesystem)(PHX_Context* context, PHX_Disk_Device* device, PHX_Filesystem_Device* outFs, PHX_Bool readonly);
    PHX_Result (*formatFilesystem)(PHX_Context* context, PHX_Disk_Device* device, PHX_Filesystem_Device* outFs, const PHX_Byte* bootsector);

    const char* name;
    const char* type;
} PHX_Filesystem_Interface;

#ifdef __cplusplus
}
#endif

#endif
