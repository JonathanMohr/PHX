#ifndef PHX_DISK_H
#define PHX_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "device/device.h"
#include "partition/partition.h"
#include "filesystem/filesystem.h"

PHX_Disk_Interface* PHX_Disk_FindInterfaceByType(const char* type);
PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_BlockDevice* fileDevice, PHX_BlockDevice* diskDeviceOut);

PHX_Partition_Interface* PHX_Partition_FindInterfaceByType(const char* type);
PHX_Result PHX_Partition_Open(PHX_Context* context, PHX_BlockDevice* diskDevice, PHX_Partition_Interface** interfaceOut, PHX_Partition_Table* partitionTableOut);

PHX_Filesystem_Interface* PHX_Filesystem_FindInterfaceByType(const char* type);
PHX_Result PHX_Filesystem_Open(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* filesystemOut);

#ifdef __cplusplus
}
#endif

#endif
