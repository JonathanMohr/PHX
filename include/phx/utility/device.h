#ifndef PHX_UTILITY_DEVICE_H
#define PHX_UTILITY_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/device/disk.h>

PHX_Bool PHX_BlockCountTransformDevice(PHX_Context* context, PHX_Disk_Device* device, PHX_SectorSize targetBlockSize, PHX_Bool takeOwnership, PHX_Disk_Device* outDevice);

#ifdef __cplusplus
}
#endif

#endif
