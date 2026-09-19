#ifndef PHX_PARTITION_PARTITION_H
#define PHX_PARTITION_PARTITION_H

#include "types.h"
#ifdef __cplusplus
extern "C" {
#endif

#include <device/device.h>

typedef struct PHX_Partition_Table
{
    PHX_BlockDevice* device;
    void* data;

    PHX_PartitionSize (*getPartitionCount)(PHX_Context* context, struct PHX_Partition_Table* table);
    PHX_PartitionSize (*getMaximumPartitionCount)(PHX_Context* context, struct PHX_Partition_Table* table);
    PHX_Bool (*getPartition)(PHX_Context* context, struct PHX_Partition_Table* table, PHX_PartitionSize index, PHX_Bool readonly, PHX_BlockDevice* out);
    PHX_Bool (*removePartition)(PHX_Context* context, struct PHX_Partition_Table* table, PHX_PartitionSize index);
    PHX_Bool (*addPartition)(PHX_Context* context, struct PHX_Partition_Table* table, PHX_PartitionSize index, PHX_BlockSize start, PHX_BlockSize size);

    void (*close)(PHX_Context* context, struct PHX_Partition_Table* table);

    const char* type;
    char name[128];
} PHX_Partition_Table;

typedef struct PHX_Partition_Interface
{
    PHX_Bool (*getTable)(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* out);
    PHX_Bool (*formatTable)(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* out, PHX_Byte* bootSector);

    const char* type;
    const char* name;
} PHX_Partition_Interface;

extern PHX_Partition_Interface* PHX_Partition_Interfaces[];
extern PHX_Size PHX_Partition_InterfaceCount;

#ifdef __cplusplus
}
#endif

#endif
