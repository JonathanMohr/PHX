#ifndef PHX_DEVICE_PARTITION_H
#define PHX_DEVICE_PARTITION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>
#include <phx/result.h>
#include <phx/context.h>
#include <phx/device/disk.h>

typedef PHX_u64 PHX_Partition_Signature_Type;
#define PHX_PARTITION_SIGNATURE_NONE 0
#define PHX_PARTITION_SIGNATURE_FIRST 1
#define PHX_PARTITION_SIGNATURE_GUID 2

typedef struct PHX_Partition_Signature
{
    PHX_u64 first;
    PHX_u64 second;
    PHX_Partition_Signature_Type type;
} PHX_Partition_Signature;

typedef PHX_u32 PHX_Partition_Flags;
#define PHX_PARTITION_FLAGS_CHANGED         (1 << 0)
#define PHX_PARTITION_FLAGS_LEGACY_BOOTABLE (1 << 1)
#define PHX_PARTITION_FLAGS_REQUIRED        (1 << 2)

typedef PHX_u32 PHX_Partition_Type;
#define PHX_PARTITION_TYPE_UNKNOWN 0

typedef struct PHX_Partition
{
    // Empty -> start = 0 and size = 0

    PHX_Partition_Signature signature;

    PHX_SectorSize start;
    PHX_SectorSize size;

    PHX_Partition_Flags flags;
    PHX_Partition_Type type;

    char name[PHX_NAME_LEN + 1];
} PHX_Partition;

typedef struct PHX_Partition_Table_Device PHX_Partition_Table_Device;

typedef struct PHX_Partition_Table_Operations
{
    PHX_Result (*writeChanges)(PHX_Partition_Table_Device* table);

    PHX_Result (*changeBootsector)(PHX_Partition_Table_Device* table, const PHX_u8* bootsector);

    // TODO: void (*getFlags)(PHX_Partition_Table_Device* pt, PHX_PartitionSize index);
    // TODO: void (*getType)(PHX_Partition_Table_Device* pt, PHX_PartitionSize index);

    void (*close)(PHX_Partition_Table_Device* table);
} PHX_Partition_Table_Operations;

struct PHX_Partition_Table_Device
{
    PHX_Partition_Signature signature;

    PHX_PartitionSize maxPartitionCount;

    PHX_SectorSize startUsable;
    PHX_SectorSize sizeUsable;

    PHX_Context* context;
    PHX_Disk_Device* disk;

    const PHX_Partition_Table_Operations* ops;

    void* data;

    PHX_u8* partitionBuffer; // Pointer to partitionSize bytes
    PHX_u8* partitions; // Points to an array of structs with a PHX_Partition as their first element
    PHX_Size partitionSize; // Size of a struct in the partitions array
};

#ifdef __cplusplus
}
#endif

#endif
