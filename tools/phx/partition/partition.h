#ifndef PHX_PARTITION_PARTITION_H
#define PHX_PARTITION_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <device/device.h>

typedef PHX_Byte PHX_Partition_Type;
#define PHX_PARTITION_UNKNOWN 0

typedef PHX_u16 PHX_Partition_Flags;
#define PHX_PARTITION_BOOTABLE (1 << 0)

typedef struct PHX_Partition
{
    PHX_BlockSize start;
    PHX_BlockSize size;

    PHX_u16 flags;

    PHX_Partition_Type type;

    char name[NAME_LEN];
} PHX_Partition;

typedef struct PHX_Partition_Table
{
    PHX_Partition* partitions;

    PHX_PartitionSize partitionCount;
    PHX_PartitionSize maxPartitionCount;

    PHX_u32 signature;

    PHX_BlockSize startUsable;
    PHX_BlockSize sizeUsable;

    PHX_Byte bootsector[512];
} PHX_Partition_Table;

typedef struct PHX_Partition_Interface
{
    PHX_Bool (*readTable)(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable);
    PHX_Bool (*writeTable)(PHX_Context* context, PHX_BlockDevice* device, const PHX_Partition_Table* table);
    void (*getDefaultTable)(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable);

    const char* type;
    const char* name;
} PHX_Partition_Interface;

extern PHX_Partition_Interface* PHX_Partition_Interfaces[];
extern PHX_Size PHX_Partition_InterfaceCount;

static inline void PHX_Partition_CloseTable(PHX_Context* context, PHX_Partition_Table* table)
{
    if (table->partitions)
        context->allocator.free(&context->allocator, table->partitions);
    table->partitions = PHX_NULL;
}

PHX_Bool PHX_Partition_CreateDevice(PHX_Context* context, PHX_BlockDevice* device, const PHX_Partition* partition, PHX_BlockDevice* outDevice);

#ifdef __cplusplus
}
#endif

#endif
