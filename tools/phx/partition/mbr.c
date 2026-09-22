#include "mbr.h"

#include <endianness.h>
#include <base.h>

#include <embed/mbr.h>

/*
    TODO: Disk signature

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

#define MBR_TYPE_EMPTY                  0x00
#define MBR_TYPE_FAT12                  0x01

#define MBR_TYPE_FAT16_BELOW_32MB       0x04
#define MBR_TYPE_EXTENDED_PARTITION_CHS 0x05
#define MBR_TYPE_FAT16_ABOVE_32MB       0x06
#define MBR_TYPE_NTFS_exFAT_HPFS        0x07

#define MBR_TYPE_FAT32_CHS              0x0B
#define MBR_TYPE_FAT32_LBA              0x0C

#define MBR_TYPE_FAT16_LBA              0x0E
#define MBR_TYPE_EXTENDED_PARTITION_LBA 0x0F

// 0x11 - 0x1F Hidden variants

#define MBR_TYPE_UNKNOWN                0x7F

#define MBR_TYPE_LINUX_SWAP             0x82
#define MBR_TYPE_LINUX                  0x83

#define MBR_TYPE_LINUX_EXTENDED         0x85

#define MBR_TYPE_LINUX_LVM              0x8E

#define MBR_TYPE_FREEBSD                0xA5
#define MBR_TYPE_OPENBSD                0xA6

#define MBR_TYPE_APPLE_DARWIN_UFS       0xA8
#define MBR_TYPE_NETBSD                 0xA9

#define MBR_TYPE_APPLE_HFS_HFS_PLUS     0xAF

#define MBR_TYPE_PROTECTED              0xEE

#define MBR_TYPE_VMWARE_VMFS            0xFB
#define MBR_TYPE_VMWARE_SWAP            0xFC
#define MBR_TYPE_LINUX_RAID_AUTODETECT  0xFD


#define MBR_TYPE "MBR-PARTITION-TABLE"

static PHX_Partition_Type MBR_ConvertType(PHX_Byte type)
{
    switch (type)
    {
        // TODO: Add Stuff
        
        default:
            return PHX_PARTITION_UNKNOWN;
    }
}

static PHX_Byte MBR_ConvertTypeBack(PHX_Partition_Type type)
{
    switch (type)
    {
        // TODO: Add Stuff

        default:
            return MBR_TYPE_UNKNOWN;
    }
}

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

static inline PHX_Bool MBR_ReadBootsector(PHX_BlockDevice* device, PHX_Byte* buffer, PHX_Size blocks)
{
    if (device->read(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}

static inline PHX_Bool MBR_WriteBootsector(PHX_BlockDevice* device, const PHX_Byte* buffer, PHX_Size blocks)
{
    if (device->write(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}

static PHX_Bool MBR_ReadTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable)
{
    const PHX_Size bootsectorBlocks = (512 + device->blockSize - 1) / device->blockSize;
    PHX_Byte* bootsectorBuffer = context->allocator.allocate(&context->allocator, bootsectorBlocks * device->blockSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    if (MBR_ReadBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
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

    // Disk signature
    PHX_u32 diskSignature;
    memcpy(&diskSignature, &bootsectorBuffer[440], 4);
    diskSignature = Endian_Convert_u32_Le(diskSignature);

    while (diskSignature == 0)
        diskSignature = PHX_Context_GetRandomU32(context);

    PHX_PartitionSize currentPartitionCount = 0;
    for (int i = 0; i < 4; i++)
    {
        PHX_Byte* partition = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte bootFlag = partition[0];
        const PHX_Byte type = partition[4];

        if (bootFlag != 0x00 && bootFlag != 0x80)
        {
            context->allocator.free(&context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (type == MBR_TYPE_PROTECTED)
        {
            context->allocator.free(&context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (type != MBR_TYPE_EMPTY)
            currentPartitionCount++;
    }

    const PHX_PartitionSize partitionCount = currentPartitionCount;
    PHX_Partition* partitions = (partitionCount > 0) ? context->allocator.allocate(&context->allocator, sizeof(PHX_Partition) * partitionCount) : PHX_NULL;
    if (partitionCount > 0 && !partitions)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    currentPartitionCount = 0;
    for (int i = 0; i < 4; i++)
    {
        PHX_Byte* partition = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_Byte bootFlag = partition[0];
        const PHX_Byte type = partition[4];
        if (type == MBR_TYPE_EMPTY)
            continue;

        PHX_Partition* part = &partitions[currentPartitionCount];
        
        PHX_u32 startLBA;
        memcpy(&startLBA, partition + 8, 4);
        startLBA = Endian_Convert_u32_Le(startLBA);

        PHX_u32 countLBA;
        memcpy(&countLBA, partition + 12, 4);
        countLBA = Endian_Convert_u32_Le(countLBA);

        part->start = startLBA;
        part->size = countLBA;

        part->flags = 0;
        if (bootFlag == 0x80)
            part->flags |= PHX_PARTITION_BOOTABLE;

        part->type = MBR_ConvertType(type);

        memset(part->name, '\0', sizeof(part->name));

        currentPartitionCount++;
    }

    const PHX_BlockSize startUsable = (1048576 + device->blockSize - 1) / device->blockSize; // 1 MiB

    outTable->partitions = partitions;
    outTable->partitionCount = partitionCount;
    outTable->maxPartitionCount = 4;
    outTable->signature = diskSignature;
    outTable->startUsable = startUsable;
    outTable->sizeUsable = (device->blockCount > startUsable) ? device->blockCount - startUsable : 0;

    context->allocator.free(&context->allocator, bootsectorBuffer);

    return PHX_TRUE;
}

static PHX_Bool MBR_WriteTable(PHX_Context* context, PHX_BlockDevice* device, const PHX_Partition_Table* table, const PHX_Byte* bootsector)
{
    if (table->partitionCount > 4)
        return PHX_FALSE;

    const PHX_Size bootsectorBlocks = (512 + device->blockSize - 1) / device->blockSize;
    PHX_Byte* bootsectorBuffer = context->allocator.allocate(&context->allocator, bootsectorBlocks * device->blockSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    // Don't destroy stuff after the bootsector
    if (bootsectorBlocks * device->blockSize > 512 && MBR_ReadBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    memset(bootsectorBuffer, 0, 512);

    // Bootsector code
    if (bootsector)
        memcpy(bootsectorBuffer, bootsector, 446);
    else
        memcpy(bootsectorBuffer, binary_file_data, 446);

    // Signature
    bootsectorBuffer[510] = 0x55;
    bootsectorBuffer[511] = 0xAA;

    // Disk signature
    const PHX_u32 diskSignature = Endian_Convert_u32_Le(table->signature);
    memcpy(&bootsectorBuffer[440], &diskSignature, 4);

    // Disk reserved
    bootsectorBuffer[444] = 0;
    bootsectorBuffer[445] = 0;

    for (PHX_PartitionSize i = 0; i < table->partitionCount; i++)
    {
        const PHX_Partition* partition = &table->partitions[i];
        PHX_Byte* part = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        if (partition->start > 0xFFFFFFFF)
        {
            context->allocator.free(&context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (partition->size > 0xFFFFFFFF)
        {
            context->allocator.free(&context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        const PHX_u32 start = Endian_Convert_u32_Le((PHX_u32)partition->start);
        const PHX_u32 count = Endian_Convert_u32_Le((PHX_u32)partition->size);
        const PHX_Byte type = MBR_ConvertTypeBack(partition->type);
        const PHX_Byte bootFlag = (partition->flags & PHX_PARTITION_BOOTABLE) ? 0x80 : 0x00;

        part[0] = bootFlag;
        MBR_EncodeCHS(1023, 254, 63, part + 1);
        part[4] = type;
        MBR_EncodeCHS(1023, 254, 63, part + 5);

        memcpy(part + 8, &start, 4);
        memcpy(part + 12, &count, 4);
    }

    if (MBR_WriteBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    context->allocator.free(&context->allocator, bootsectorBuffer);

    return PHX_TRUE;
}

static void MBR_DefaultTable(PHX_Context* context, PHX_BlockDevice* device, PHX_Partition_Table* outTable)
{
    (void)context;
    const PHX_BlockSize startUsable = (1048576 + device->blockSize - 1) / device->blockSize; // 1 MiB

    outTable->partitions = PHX_NULL;
    outTable->partitionCount = 0;
    outTable->maxPartitionCount = 4;
    outTable->signature = PHX_Context_GetRandomU32(context);
    outTable->startUsable = startUsable;
    outTable->sizeUsable = (device->blockCount > startUsable) ? device->blockCount - startUsable : 0;
}

PHX_Partition_Interface PHX_Partition_MBR_Interface = {
    MBR_ReadTable,
    MBR_WriteTable,
    MBR_DefaultTable,
    MBR_TYPE,
    "MBR-PARTITION-TABLE-INTERFACE"
};
