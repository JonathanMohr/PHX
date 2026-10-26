#include "gpt.h"
#include "types.h"

#include <endianness.h>
#include <base.h>

#include <embed/gpt.h>

/*

    Protective MBR:
        Bootcode (440 bytes)
        Disk signature (4 bytes)
        reserved (2 bytes)
        entry (16 bytes)
        other 3 empty entries (48 bytes)
        signature (2 bytes)

*/

#define GPT_TYPE "GPT-PARTITION-TABLE"


static inline void MBR_EncodeCHS(PHX_u16 cylinder, PHX_Byte head, PHX_Byte sector, PHX_Byte out[3])
{
    if (cylinder > 1023 || sector > 63 || sector == 0)
    {
        cylinder = 1023;
        head = 254;
        sector = 63;
    }

    out[0] = head;
    out[1] = (PHX_Byte)(((cylinder >> 2) & 0xC0) | (sector & 0x3F));
    out[2] = (PHX_Byte)(cylinder & 0xFF);
}


static PHX_Bool GPT_ReadTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable)
{
    const PHX_Size bootsectorBlocks = (512 + device->sectorSize - 1) / device->sectorSize;
    PHX_Byte* bootsectorBuffer = context->allocator.allocate(&context->allocator, bootsectorBlocks * device->sectorSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    if (device->read(device, bootsectorBuffer, 0, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    const PHX_Byte signature[2] = {bootsectorBuffer[510], bootsectorBuffer[511]};
    if (signature[0] != 0x55 || signature[1] != 0xAA)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    PHX_Bool found = PHX_FALSE;
    for (int i = 0; i < 4; i++)
    {
        PHX_Byte* partition = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte type = partition[4];

        PHX_u32 startLBA;
        memcpy(&startLBA, partition + 8, 4);
        startLBA = Endian_Convert_u32_Le(startLBA);

        if (type == 0xEE && startLBA == 1)
        {
            found = PHX_TRUE;
            break;
        }
    }

    if (found != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    // TODO
    (void)outTable;
    context->allocator.free(&context->allocator, bootsectorBuffer);
    return PHX_FALSE;
}

static PHX_Bool GPT_WriteTable(PHX_Context* context, PHX_BlockDevice* device, const PHX_Partition_Table* table)
{
    // TODO
    (void)MBR_EncodeCHS;
    (void)context;
    (void)device;
    (void)table;
    return PHX_FALSE;
}

static PHX_Bool GPT_DefaultTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable)
{
    const PHX_SectorSize sectorSize = device->sectorSize;
    const PHX_SectorSize bootsectorSectors = (512 + sectorSize - 1) / sectorSize;

    const PHX_PartitionSize maxPartitionCount = 128;
    const PHX_PartitionSize partitionSectors = (maxPartitionCount * 128 + sectorSize - 1) / sectorSize;

    const PHX_PartitionSize infoSectors = bootsectorSectors + partitionSectors;

    if (device->totalSectors <= bootsectorSectors)
        return PHX_FALSE;

    outTable->partitions = PHX_NULL;
    outTable->partitionCount = 0;
    outTable->maxPartitionCount = maxPartitionCount;
    outTable->signature.first = 0;
    while (outTable->signature.first == 0)
        outTable->signature.first = PHX_Context_GetRandomU64(context);
    outTable->signature.second = 0;
    while (outTable->signature.second == 0)
        outTable->signature.second = PHX_Context_GetRandomU64(context);
    outTable->startUsable = infoSectors;
    outTable->sizeUsable = device->totalSectors - infoSectors * 2;
    memcpy(outTable->bootsector, binary_file_data, 512);

    return PHX_TRUE;
}

PHX_Partition_Interface PHX_Partition_GPT_Interface = {
    GPT_ReadTable,
    GPT_WriteTable,
    GPT_DefaultTable,
    GPT_TYPE,
    "GPT-PARTITION-TABLE-INTERFACE",
    "gpt"
};
