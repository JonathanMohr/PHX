#include "mbr.h"
#include "device/device.h"
#include "partition/partition.h"
#include "types.h"
#include <base.h>

/*
    Partition (16 byte):
        u8 bootFlag
        u8 chsFirst[3]
        u8 type
        u8 chsLast[3]
        u32 startLBA
        u32 sectorCount

    MBR Bootsector:
        u8 code[446]
        Partition partitions[4]
        u8 signature[2]
*/

#define MBR_TYPE_EMPTY 0x00
#define MBR_TYPE_PROTECTED 0xEE

#define MBR_Type "MBR-PARTITION-TABLE"

typedef struct
{
    PHX_Byte* blockBuffer; // Only set if bootSectorBlock * blockSize is more than 512
    PHX_Byte* bootSectorBuffer;
    PHX_Size bootSectorBlocks;
} MBR_Data;

static inline PHX_Bool MBR_ReadBootsector(PHX_BlockDevice* device, PHX_Byte* blockBuffer, PHX_Byte* buffer, PHX_Size blocks)
{
    (void)blockBuffer;

    if (device->read(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}

static inline PHX_Bool MBR_WriteBootsector(PHX_BlockDevice* device, PHX_Byte* blockBuffer, PHX_Byte* buffer, PHX_Size blocks)
{
    if (blockBuffer)
    {
        if (device->read(device, blockBuffer, blocks - 1, 1) != 1)
            return PHX_FALSE;

        memcpy(buffer + 512, blockBuffer + (512 - (blocks - 1) * device->blockSize), blocks * device->blockSize - 512);
    }

    if (device->write(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}


static PHX_PartitionSize MBR_GetPartitionCount(PHX_Context* context, PHX_Partition_Table* table)
{
    (void)context;

    MBR_Data* data = table->data;

    PHX_PartitionSize count = 0;
    for (int i = 0; i < 4; i++)
    {
        PHX_Byte* partition = data->bootSectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte type = partition[4];
        if (type != MBR_TYPE_EMPTY)
            count++;
    }

    return count;
}

static PHX_PartitionSize MBR_GetMaximumPartitionCount(PHX_Context* context, PHX_Partition_Table* table)
{
    (void)context;
    (void)table;
    return 4;
}

static PHX_Bool MBR_GetPartition(PHX_Context* context, PHX_Partition_Table* table, PHX_PartitionSize index, PHX_Bool readonly, PHX_BlockDevice* out)
{
    (void)context;

    MBR_Data* data = table->data;

    PHX_Byte* partition;
    for (int i = 0; i < 4; i++)
    {
        partition = data->bootSectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte type = partition[4];
        if (type != MBR_TYPE_EMPTY)
        {
            index--;
            if (index == 0)
                break;
        }
    }

    if (index != 0)
        return PHX_FALSE;

    // TODO
}

static PHX_Bool MBR_RemovePartition(PHX_Context* context, PHX_Partition_Table* table, PHX_PartitionSize index)
{
    (void)context;

    MBR_Data* data = table->data;

    PHX_Byte* partition;
    for (int i = 0; i < 4; i++)
    {
        partition = data->bootSectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte type = partition[4];
        if (type != MBR_TYPE_EMPTY)
        {
            index--;
            if (index == 0)
                break;
        }
    }

    if (index != 0)
        return PHX_FALSE;

    // TODO
}

static PHX_Bool MBR_AddPartition(PHX_Context* context, PHX_Partition_Table* table, PHX_PartitionSize index, PHX_BlockSize start, PHX_BlockSize size)
{
    (void)context;

    MBR_Data* data = table->data;

    PHX_Byte* partition;
    for (int i = 0; i < 4; i++)
    {
        partition = data->bootSectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte type = partition[4];
        if (type != MBR_TYPE_EMPTY)
        {
            index--;
            if (index == 0)
                break;
        }
    }

    if (index != 0)
        return PHX_FALSE;

    // TODO
}


static void MBR_Close(PHX_Context* context, PHX_Partition_Table* table)
{
    MBR_Data* data = table->data;

    if (MBR_WriteBootsector(table->device, data->blockBuffer, data->bootSectorBuffer, data->bootSectorBlocks) != PHX_TRUE)
    {
        // TODO
    }

    context->allocator.free(&context->allocator, data->bootSectorBuffer);
    context->allocator.free(&context->allocator, data);
}


static inline MBR_Data* MBR_Allocate(PHX_Context* context, PHX_BlockDevice* device)
{
    MBR_Data* data = context->allocator.allocate(&context->allocator, sizeof(MBR_Data));
    if (!data)
        return PHX_NULL;

    const PHX_Size bootSectorBlocks = (512 + device->blockSize - 1) / device->blockSize;

    if (bootSectorBlocks * device->blockSize == 512)
    {
        data->bootSectorBlocks = bootSectorBlocks;
        data->bootSectorBuffer = context->allocator.allocate(&context->allocator, device->blockSize * bootSectorBlocks);
        if (!data->bootSectorBuffer)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_NULL;
        }
    }
    else
    {
        PHX_Byte* allocated = context->allocator.allocate(&context->allocator, device->blockSize * bootSectorBlocks + device->blockSize);
        if (!allocated)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_NULL;
        }
        
        data->bootSectorBlocks = bootSectorBlocks;
        data->bootSectorBuffer = allocated;

        data->blockBuffer = allocated + device->blockSize * bootSectorBlocks;
    }

    return data;
}

static PHX_Bool MBR_GetTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* out)
{
    MBR_Data* data = MBR_Allocate(context, device);
    if (!data)
        return PHX_FALSE;

    if (MBR_ReadBootsector(device, data->blockBuffer, data->bootSectorBuffer, data->bootSectorBlocks) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, data->bootSectorBuffer);
        context->allocator.free(&context->allocator, data);
        return PHX_FALSE;
    }

    const PHX_Byte signature[2] = {data->bootSectorBuffer[510], data->bootSectorBuffer[511]};
    if (signature[0] != 0x55 || signature[1] != 0xAA)
    {
        context->allocator.free(&context->allocator, data->bootSectorBuffer);
        context->allocator.free(&context->allocator, data);
        return PHX_FALSE;
    }

    for (int i = 0; i < 4; i++)
    {
        PHX_Byte* partition = data->bootSectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte bootFlag = partition[0];
        const PHX_Byte type = partition[4];

        if (bootFlag != 0x00 && bootFlag != 0x80)
        {
            context->allocator.free(&context->allocator, data->bootSectorBuffer);
            context->allocator.free(&context->allocator, data);
            return PHX_FALSE;
        }

        if (type == MBR_TYPE_PROTECTED)
        {
            context->allocator.free(&context->allocator, data->bootSectorBuffer);
            context->allocator.free(&context->allocator, data);
            return PHX_FALSE;
        }
    }

    out->device = device;
    out->data = data;

    out->getPartitionCount = MBR_GetPartitionCount;
    out->getMaximumPartitionCount = MBR_GetMaximumPartitionCount;
    out->getPartition = MBR_GetPartition;
    out->removePartition = MBR_RemovePartition;
    out->addPartition = MBR_AddPartition;

    out->close = MBR_Close;

    out->type = MBR_Type;
    memcpy(out->name, device->name, NAME_LEN);
    // TODO: Maybe better name

    return PHX_TRUE;
}

static PHX_Bool MBR_FormatTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* out, PHX_Byte* bootSector)
{
    MBR_Data* data = MBR_Allocate(context, device);
    if (!data)
        return PHX_FALSE;

    memset(data->bootSectorBuffer, 0, 512);

    // Signature
    data->bootSectorBuffer[510] = 0x55;
    data->bootSectorBuffer[511] = 0xAA;

    // BootSector
    if (bootSector)
        memcpy(data->bootSectorBuffer, bootSector, 446);

    out->device = device;
    out->data = data;

    out->getPartitionCount = MBR_GetPartitionCount;
    out->getMaximumPartitionCount = MBR_GetMaximumPartitionCount;
    out->getPartition = MBR_GetPartition;
    out->removePartition = MBR_RemovePartition;
    out->addPartition = MBR_AddPartition;

    out->close = MBR_Close;

    out->type = MBR_Type;
    memcpy(out->name, device->name, NAME_LEN);
    // TODO: Maybe better name

    return PHX_TRUE;
}

PHX_Partition_Interface PHX_Partition_MBR_Interface = {
    MBR_GetTable,
    MBR_FormatTable,
    MBR_Type,
    "MBR-PARTITION-TABLE-INTERFACE"
};
