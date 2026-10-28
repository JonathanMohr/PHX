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
#define PHX_PARTITION_FLAGS_CHANGED         ((PHX_Partition_Flags)(1 << 0))
#define PHX_PARTITION_FLAGS_LEGACY_BOOTABLE ((PHX_Partition_Flags)(1 << 1))
#define PHX_PARTITION_FLAGS_REQUIRED        ((PHX_Partition_Flags)(1 << 2))

typedef PHX_u32 PHX_Partition_Type;
#define PHX_PARTITION_TYPE_EMPTY    0
#define PHX_PARTITION_TYPE_UNKNOWN  1
#define PHX_PARTITION_TYPE_SPECIFIC 2

typedef struct PHX_Partition
{
    PHX_Partition_Signature signature;

    PHX_SectorSize start;
    PHX_SectorSize size;

    PHX_Partition_Flags flags;
    PHX_Partition_Type type;

    char name[PHX_NAME_LEN + 1];
} PHX_Partition;

typedef struct PHX_Partition_Type_Info
{
    const char** names;
} PHX_Partition_Type_Info;
#define PHX_PARTITION_TYPE_INFO_NAMES(...) (const char*[]){ __VA_ARGS__, NULL }

typedef struct PHX_Partition_Table_Device PHX_Partition_Table_Device;

typedef struct PHX_Partition_Table_Operations
{
    PHX_Result (*writeChanges)(PHX_Partition_Table_Device* table);

    PHX_Result (*changeBootsector)(PHX_Partition_Table_Device* table, const PHX_u8* bootsector);

    // TODO: Signature

    // TODO: void (*getFlags)(PHX_Partition_Table_Device* table, PHX_PartitionSize index);

    PHX_Bool (*setType)(PHX_Partition_Table_Device* table, PHX_PartitionSize index, const char* typeName);

    const char* (*getTypeName)(PHX_Partition_Table_Device* table, PHX_PartitionSize index);
    void (*getSpecificType)(PHX_Partition_Table_Device* table, PHX_PartitionSize index, char* typeOut);

    void (*close)(PHX_Partition_Table_Device* table);

    PHX_Partition_Type_Info* typeInfos;
    PHX_Size typeCount;

    PHX_Size specificTypeLen;
} PHX_Partition_Table_Operations;

struct PHX_Partition_Table_Device
{
    PHX_Partition_Signature signature;

    PHX_PartitionSize partitionSlots;

    PHX_SectorSize startUsable;
    PHX_SectorSize sizeUsable;

    PHX_Context* context;
    PHX_Disk_Device* disk;

    const PHX_Partition_Table_Operations* ops;
    const char* type;

    void* data;

    PHX_u8* partitionBuffer; // Pointer to partitionSize bytes
    PHX_u8* partitions; // Points to an array of structs with a PHX_Partition as their first element
    PHX_Size partitionSize; // Size of a struct in the partitions array
};

static inline PHX_Partition* PHX_Partition_Table_GetPartition(PHX_Partition_Table_Device* table, PHX_PartitionSize index)
{
    PHX_u8* partition = table->partitions + table->partitionSize * index;
    return (PHX_Partition*)partition;
}

#ifdef __cplusplus
}
#endif

#endif
