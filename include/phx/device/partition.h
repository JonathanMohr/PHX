#ifndef PHX_DEVICE_PARTITION_H
#define PHX_DEVICE_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>
#include <phx/context.h>

typedef struct PHX_Partition_Signature
{
    PHX_u64 first;
    PHX_u64 second;
    // TODO: exists bool
} PHX_Partition_Signature;

typedef PHX_u8 PHX_Partition_Type;
#define PHX_PARTITION_UNKNOWN 0
#define PHX_PARTITION_FAT12   1
#define PHX_PARTITION_FAT16   2
#define PHX_PARTITION_FAT32   3

typedef PHX_u16 PHX_Partition_Flags;
#define PHX_PARTITION_BOOTABLE (1 << 0)

typedef struct PHX_Partition
{
    PHX_SectorSize start;
    PHX_SectorSize size;

    PHX_Partition_Signature signature;

    PHX_u16 flags;

    PHX_Partition_Type type;

    char name[PHX_NAME_LEN + 1];
} PHX_Partition;

typedef struct PHX_Partition_Table
{
    PHX_Partition* partitions;

    PHX_PartitionSize partitionCount;
    PHX_PartitionSize maxPartitionCount;

    PHX_Partition_Signature signature;

    PHX_SectorSize startUsable;
    PHX_SectorSize sizeUsable;

    PHX_u8 bootsector[512];
} PHX_Partition_Table;

static inline void PHX_Partition_CloseTable(PHX_Context* context, PHX_Partition_Table* table)
{
    if (table->partitions)
        context->allocator->free(context->allocator, table->partitions);
    table->partitions = PHX_NULL;
}

#ifdef __cplusplus
}
#endif

#endif
