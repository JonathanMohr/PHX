#ifndef PHX_DISK_H
#define PHX_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "device/device.h"

PHX_Disk_Interface* PHX_Disk_FindInterfaceByType(const char* type);
PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_BlockDevice* fileDevice, PHX_BlockDevice* diskDeviceOut);

#ifdef __cplusplus
}
#endif

#endif
