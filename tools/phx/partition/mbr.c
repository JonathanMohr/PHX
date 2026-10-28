#include "mbr.h"
#include "phx/base/bytes.h"
#include "phx/context.h"
#include "phx/device/disk.h"
#include "phx/device/partition.h"
#include "phx/result.h"
#include "phx/types.h"

#include <phx/base/serialization.h>
#include <phx/base/intrinsic.h>

#include <embed/mbr.h>

/*
    MBR Bootsector:
        u8 code[440]            (440 bytes)
        Disk signature          (4 bytes)
        reserved                (2 bytes)
        Partition partitions[4] (64 bytes)
        u8 signature[2]         (2 bytes)

    Partition (16 bytes):
        u8 bootFlag             (1 byte)
        u8 chsFirst[3]          (3 bytes)
        u8 type                 (1 byte)
        u8 chsLast[3]           (3 bytes)
        u32 startLBA            (4 bytes)
        u32 sectorCount         (4 bytes)
*/

#define MBR_BOOTSECTOR_DSIG 440
#define MBR_BOOTSECTOR_PART 446
#define MBR_BOOTSECTOR_SIG  510

#define MBR_PARTITION_BFL 0
#define MBR_PARTITION_CHSF 1
#define MBR_PARTITION_TYP 4
#define MBR_PARTITION_CHSL 5
#define MBR_PARTITION_STL 8
#define MBR_PARTITION_SEC 12

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


typedef struct
{
    PHX_Partition partition;
    PHX_u8 type;
} MBR_Partition;


typedef struct
{
    PHX_u8* bootsectorBuffer;
    PHX_u8 bootsector[512];
} MBR_Data;


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


static void MBR_FreeData(PHX_Context* context, MBR_Data* data)
{
    context->allocator->free(context->allocator, data);
}

static MBR_Data* MBR_AllocateData(PHX_Context* context, PHX_SectorSize bootsectorSectors, PHX_SectorSize sectorSize)
{
    MBR_Data* data = context->allocator->allocate(context->allocator, sizeof(MBR_Data) + bootsectorSectors * sectorSize);
    if (!data) return PHX_NULL;
    data->bootsectorBuffer = (PHX_u8*)(data + 1);
    return data;
}


static PHX_Result MBR_ChangeBootsector(PHX_Partition_Table_Device* table, const PHX_u8* bootsector)
{
    MBR_Data* data = table->data;
    memcpy(data->bootsector, bootsector, 512);
    return PHX_SUCCESS;
}


static PHX_Result MBR_WriteChanges(PHX_Partition_Table_Device* table)
{
    // TODO
    (void)MBR_EncodeCHS;
    (void)table;
    return PHX_ERROR_INTERNAL;
}


static void MBR_Close(PHX_Partition_Table_Device* table)
{
    MBR_Data* data = table->data;
    MBR_FreeData(table->context, data);
    table->context->allocator->free(table->context->allocator, table->partitionBuffer);
    table->context->allocator->free(table->context->allocator, table->partitions);
}


static const PHX_Partition_Table_Operations MBR_Operations = {
    MBR_WriteChanges,
    MBR_ChangeBootsector,
    MBR_Close
};


static const char sPartitionName[] = "MBR Partition ";
static const PHX_Size sPartitionNameToCopy = (sizeof(sPartitionName) > PHX_NAME_LEN + 1) ? PHX_NAME_LEN + 1 : sizeof(sPartitionName);

static PHX_Result MBR_OpenPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable)
{
    const PHX_SectorSize bootsectorSectors = (512 + device->sectorSize - 1) / device->sectorSize;

    MBR_Data* data = MBR_AllocateData(context, bootsectorSectors, device->sectorSize);
    if (!data) return PHX_ERROR_MEMORY;

    if (device->read(device, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
    {
        MBR_FreeData(context, data);
        return PHX_ERROR_IO;
    }

    memcpy(data->bootsector, data->bootsectorBuffer, 512);

    const PHX_u16 signature = PHX_Read_Le_u16(data->bootsector + MBR_BOOTSECTOR_SIG);
    if (signature != 0xAA55)
    {
        MBR_FreeData(context, data);
        return PHX_ERROR_FORMAT;
    }

    const PHX_u32 diskSignature = PHX_Read_Le_u32(data->bootsector + MBR_BOOTSECTOR_DSIG);

    for (int i = 0; i < 4; i++)
    {
        PHX_u8* partition = data->bootsector + MBR_BOOTSECTOR_PART + (PHX_Size)i * 16;

        const PHX_u8 bootFlag = PHX_Read_Le_u8(partition + MBR_PARTITION_BFL);
        const PHX_u8 type = PHX_Read_Le_u8(partition + MBR_PARTITION_TYP);

        if (bootFlag != 0x00 && bootFlag != 0x80)
        {
            MBR_FreeData(context, data);
            return PHX_ERROR_FORMAT;
        }

        if (type == MBR_TYPE_PROTECTED)
        {
            MBR_FreeData(context, data);
            return PHX_ERROR_FORMAT;
        }
    }

    MBR_Partition* partitions = context->allocator->allocate(context->allocator, sizeof(MBR_Partition) * 4);
    if (!partitions)
    {
        MBR_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    PHX_u8* partitionBuffer = context->allocator->allocate(context->allocator, sizeof(MBR_Partition));
    if (!partitionBuffer)
    {
        context->allocator->free(context->allocator, partitions);
        MBR_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    for (int i = 0; i < 4; i++)
    {
        MBR_Partition* partition = &partitions[i];
        PHX_u8* rawPartition = data->bootsector + MBR_BOOTSECTOR_PART + (PHX_Size)i * 16;

        const PHX_u8 bootFlag = PHX_Read_Le_u8(rawPartition + MBR_PARTITION_BFL);
        const PHX_u8 type = PHX_Read_Le_u8(rawPartition + MBR_PARTITION_TYP);
        const PHX_u32 startLba = PHX_Read_Le_u32(rawPartition + MBR_PARTITION_STL);
        const PHX_u32 sectorCount = PHX_Read_u32(rawPartition + MBR_PARTITION_SEC);

        partition->partition.signature.first = (PHX_u64)i;
        partition->partition.signature.second = 0;
        partition->partition.signature.type = PHX_PARTITION_SIGNATURE_FIRST;

        partition->partition.start = startLba;
        partition->partition.size = sectorCount;

        partition->partition.flags = 0;
        if (bootFlag == 0x80)
            partition->partition.flags |= PHX_PARTITION_FLAGS_LEGACY_BOOTABLE;

        partition->partition.type = PHX_PARTITION_TYPE_UNKNOWN; // TODO

        memcpy(partition->partition.name, sPartitionName, sPartitionNameToCopy);
        if (sizeof(partition->partition.name) > sizeof(sPartitionName))
        {
            partition->partition.name[sizeof(sPartitionName) - 1] = (char)('0' + (unsigned char)i + 1);
            partition->partition.name[sizeof(sPartitionName)] = '\0';
        }
        else
            partition->partition.name[sizeof(partition->partition.name) - 1] = '\0';


        partition->type = type;
    }

    outTable->signature.first = diskSignature;
    outTable->signature.second = 0;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_FIRST;

    outTable->maxPartitionCount = 4;

    outTable->startUsable = 1;
    outTable->sizeUsable = device->totalSectors - 1;

    outTable->context = context;
    outTable->disk = device;

    outTable->ops = &MBR_Operations;

    outTable->data = data;

    outTable->partitionBuffer = partitionBuffer;
    outTable->partitions = (PHX_u8*)partitions;
    outTable->partitionSize = sizeof(MBR_Partition);
    
    return PHX_SUCCESS;
}

static PHX_Result MBR_FormatPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable, const PHX_u8* bootsector)
{
    // TODO
    (void)context;
    (void)device;
    (void)outTable;
    (void)bootsector;
    return PHX_ERROR_INTERNAL;
}


PHX_Partition_Interface PHX_Partition_MBR_Interface = {
    MBR_OpenPartitionTable,
    MBR_FormatPartitionTable,
    MBR_TYPE,
    "MBR-PARTITION-TABLE-INTERFACE",
    "mbr"
};


/*
static PHX_Partition_Type MBR_ConvertType(PHX_Byte type)
{
    switch (type)
    {
        case MBR_TYPE_FAT12:
            return PHX_PARTITION_FAT12;

        case MBR_TYPE_FAT16_BELOW_32MB: case MBR_TYPE_FAT16_ABOVE_32MB:
        case MBR_TYPE_FAT16_LBA:
            return PHX_PARTITION_FAT16;

        case MBR_TYPE_FAT32_CHS: case MBR_TYPE_FAT32_LBA:
            return PHX_PARTITION_FAT32;
        
        default:
            return PHX_PARTITION_UNKNOWN;
    }
}

static PHX_Byte MBR_ConvertTypeBack(PHX_Partition_Type type)
{
    switch (type)
    {
        case PHX_PARTITION_FAT12:
            return MBR_TYPE_FAT12;

        case PHX_PARTITION_FAT16:
            return MBR_TYPE_FAT16_LBA;

        case PHX_PARTITION_FAT32:
            return MBR_TYPE_FAT32_LBA;

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

static inline PHX_Bool MBR_ReadBootsector(PHX_Disk_Device* device, PHX_Byte* buffer, PHX_Size blocks)
{
    if (device->read(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}

static inline PHX_Bool MBR_WriteBootsector(PHX_Disk_Device* device, const PHX_Byte* buffer, PHX_Size blocks)
{
    if (device->write(device, buffer, 0, blocks) != blocks)
        return PHX_FALSE;

    return PHX_TRUE;
}

static PHX_Bool MBR_ReadTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table* outTable)
{
    const PHX_Size bootsectorBlocks = (512 + device->sectorSize - 1) / device->sectorSize;
    PHX_Byte* bootsectorBuffer = context->allocator->allocate(context->allocator, bootsectorBlocks * device->sectorSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    if (MBR_ReadBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    const PHX_Byte signature[2] = {bootsectorBuffer[510], bootsectorBuffer[511]};
    if (signature[0] != 0x55 || signature[1] != 0xAA)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
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
            context->allocator->free(context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (type == MBR_TYPE_PROTECTED)
        {
            context->allocator->free(context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (type != MBR_TYPE_EMPTY)
            currentPartitionCount++;
    }

    const PHX_PartitionSize partitionCount = currentPartitionCount;
    PHX_Partition* partitions = (partitionCount > 0) ? context->allocator->allocate(context->allocator, sizeof(PHX_Partition) * partitionCount) : PHX_NULL;
    if (partitionCount > 0 && !partitions)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
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

        part->signature.first = (PHX_u64)i;
        part->signature.second = 0;
        part->signature.type = PHX_PARTITION_SIGNATURE_FIRST;

        part->flags = 0;
        if (bootFlag == 0x80)
            part->flags |= PHX_PARTITION_BOOTABLE;

        part->type = MBR_ConvertType(type);

        memset(part->name, '\0', sizeof(part->name));

        currentPartitionCount++;
    }

    outTable->partitions = partitions;
    outTable->partitionCount = partitionCount;
    outTable->maxPartitionCount = 4;
    outTable->signature.first = diskSignature;
    outTable->signature.second = 0;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_FIRST;
    outTable->startUsable = 1;
    outTable->sizeUsable = device->totalSectors - 1;

    memcpy(outTable->bootsector, bootsectorBuffer, 512);

    context->allocator->free(context->allocator, bootsectorBuffer);

    return PHX_TRUE;
}

static PHX_Bool MBR_WriteTable(PHX_Context* context, PHX_Disk_Device* device, const PHX_Partition_Table* table)
{
    if (table->partitionCount > 4)
        return PHX_FALSE;

    const PHX_Size bootsectorBlocks = (512 + device->sectorSize - 1) / device->sectorSize;
    PHX_Byte* bootsectorBuffer = context->allocator->allocate(context->allocator, bootsectorBlocks * device->sectorSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    // Don't destroy stuff after the bootsector
    if (bootsectorBlocks * device->sectorSize > 512 && MBR_ReadBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    // TODO: Warn about multiple bootable partitions
    // TODO: Maybe warn if start + size overflow 32-bit
    // TODO: Warn about images above 2T

    memset(bootsectorBuffer, 0, 512);

    // Bootsector code
    memcpy(bootsectorBuffer, table->bootsector, 440);

    // Signature
    bootsectorBuffer[510] = 0x55;
    bootsectorBuffer[511] = 0xAA;

    // Disk signature
    const PHX_u32 diskSignature = Endian_Convert_u32_Le((PHX_u32)table->signature.first);
    memcpy(&bootsectorBuffer[440], &diskSignature, 4);

    // Disk reserved
    bootsectorBuffer[444] = 0;
    bootsectorBuffer[445] = 0;

    const PHX_u32 sectorsPerCylinder = (PHX_u32)device->totalHeads * (PHX_u32)device->sectorsPerTrack;

    for (PHX_PartitionSize i = 0; i < table->partitionCount; i++)
    {
        const PHX_Partition* partition = &table->partitions[i];
        PHX_Byte* part = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        if (partition->start > 0xFFFFFFFF)
        {
            context->allocator->free(context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        if (partition->size > 0xFFFFFFFF)
        {
            context->allocator->free(context->allocator, bootsectorBuffer);
            return PHX_FALSE;
        }

        const PHX_u32 start = Endian_Convert_u32_Le((PHX_u32)partition->start);
        const PHX_u32 count = Endian_Convert_u32_Le((PHX_u32)partition->size);
        const PHX_Byte type = MBR_ConvertTypeBack(partition->type);
        const PHX_Byte bootFlag = (partition->flags & PHX_PARTITION_BOOTABLE) ? 0x80 : 0x00;

        const PHX_SectorSize firstCylinder = partition->start / sectorsPerCylinder;
        const PHX_u8 firstHead = (PHX_u8)((partition->start / (PHX_SectorSize)device->sectorsPerTrack) % (PHX_SectorSize)device->totalHeads);
        const PHX_u8 firstSector = (PHX_u8)((partition->start % (PHX_SectorSize)device->sectorsPerTrack) + 1);

        const PHX_SectorSize lastCylinder = (partition->start + partition->size - 1) / sectorsPerCylinder;
        const PHX_u8 lastHead = (PHX_u8)(((partition->start + partition->size - 1) / (PHX_SectorSize)device->sectorsPerTrack) % (PHX_SectorSize)device->totalHeads);
        const PHX_u8 lastSector = (PHX_u8)(((partition->start + partition->size - 1) % (PHX_SectorSize)device->sectorsPerTrack) + 1);

        part[0] = bootFlag;
        if (firstCylinder <= 1023)
            MBR_EncodeCHS((PHX_u16)firstCylinder, firstHead, firstSector, part + 1);
        else
            MBR_EncodeCHS(1023, 254, 63, part + 1);
        part[4] = type;
        if (lastCylinder <= 1023)
            MBR_EncodeCHS((PHX_u16)lastCylinder, lastHead, lastSector, part + 5);
        else
            MBR_EncodeCHS(1023, 254, 63, part + 5);

        memcpy(part + 8, &start, 4);
        memcpy(part + 12, &count, 4);
    }

    if (MBR_WriteBootsector(device, bootsectorBuffer, bootsectorBlocks) != PHX_TRUE)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    context->allocator->free(context->allocator, bootsectorBuffer);

    return PHX_TRUE;
}

static PHX_Bool MBR_DefaultTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table* outTable)
{
    const PHX_SectorSize sectorSize = device->sectorSize;
    const PHX_SectorSize bootsectorSectors = (512 + sectorSize - 1) / sectorSize;

    if (device->totalSectors <= bootsectorSectors)
        return PHX_FALSE;

    outTable->partitions = PHX_NULL;
    outTable->partitionCount = 0;
    outTable->maxPartitionCount = 4;
    outTable->signature.first = PHX_Context_GetRandomU32(context);
    outTable->signature.second = 0;
    while (outTable->signature.first == 0)
        outTable->signature.first = PHX_Context_GetRandomU32(context);
    outTable->startUsable = bootsectorSectors;
    outTable->sizeUsable = device->totalSectors - bootsectorSectors;
    memcpy(outTable->bootsector, binary_file_data, 512);

    return PHX_TRUE;
}
*/

