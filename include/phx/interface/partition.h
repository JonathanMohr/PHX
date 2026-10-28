#ifndef PHX_INTERFACE_PARTITION_H
#define PHX_INTERFACE_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/device/disk.h>
#include <phx/device/partition.h>

typedef struct PHX_Partition_Interface
{
    PHX_Result (*openPartitionTable)(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable);
    PHX_Result (*formatPartitionTable)(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable, const PHX_u8* bootsector);

    const PHX_Partition_Table_Operations* ops;
    
    const char* deviceType;
    const char* name;
    const char* type;
} PHX_Partition_Interface;

#ifdef __cplusplus
}
#endif

#endif
