#ifndef PHX_UTILITY_PARTITION_H
#define PHX_UTILITY_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/device/disk.h>
#include <phx/device/partition.h>

PHX_Bool PHX_Partition_CreateDevice(PHX_Context* context, PHX_Disk_Device* device, const PHX_Partition* partition, PHX_Disk_Device* outDevice);

#ifdef __cplusplus
}
#endif

#endif
