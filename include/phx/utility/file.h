#ifndef PHX_UTILITY_FILE_H
#define PHX_UTILITY_FILE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/device/file.h>
#include <phx/device/disk.h>

void PHX_File_GetDiskDevice(PHX_File_Device* fileDevice, PHX_Disk_Device* out);

#ifdef __cplusplus
}
#endif

#endif
