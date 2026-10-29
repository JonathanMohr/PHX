#ifndef PHX_DISK_H
#define PHX_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/interface/disk.h>
#include <phx/interface/partition.h>
#include <phx/interface/filesystem.h>

PHX_Disk_Interface* PHX_Disk_FindInterfaceByType(const char* type);
PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_File_Device* fileDevice, PHX_Disk_Device* diskDeviceOut, PHX_Bool readonly);

PHX_Partition_Interface* PHX_Partition_FindInterfaceByType(const char* type);
PHX_Result PHX_Partition_Open(PHX_Context* context, PHX_Disk_Device* diskDevice, PHX_Partition_Table_Device* partitionTableOut);

PHX_Filesystem_Interface* PHX_Filesystem_FindInterfaceByType(const char* type);
PHX_Result PHX_Filesystem_Open(PHX_Context* context, PHX_Disk_Device* device, PHX_Filesystem_Device* filesystemOut);

#ifdef __cplusplus
}
#endif

#endif
