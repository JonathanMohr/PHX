#include "mbr.h"
#include "phx/device/partition.h"
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

#define MBR_SIGNATURE 0xAA55

#define MBR_BOOTSECTOR_DSIG 440
#define MBR_BOOTSECTOR_RES 444
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


#define MBR_SPECIFIC_TYPE_LEN 5
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

static PHX_Partition_Type_Info MBR_TypeInfos[] = {
    {
        PHX_PARTITION_TYPE_INFO_NAMES("unknown")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("fat12")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("fat16")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("fat32")
    }
};

static PHX_u8 MBR_GetTypeByName(const char* name)
{
    if (strcmp(name, "unknown") == 0)
        return MBR_TYPE_UNKNOWN;

    if (strcmp(name, "fat12") == 0)
        return MBR_TYPE_FAT12;

    if (strcmp(name, "fat16") == 0)
        return MBR_TYPE_FAT16_LBA;

    if (strcmp(name, "fat32") == 0)
        return MBR_TYPE_FAT32_LBA;

    return 0;
}


static const char* MBR_GetTypeName(PHX_u8 type)
{
    switch (type)
    {
        case MBR_TYPE_FAT12: return "FAT12";
        case MBR_TYPE_FAT16_BELOW_32MB: return "FAT16 CHS";
        case MBR_TYPE_EXTENDED_PARTITION_CHS: return "Extended Partition CHS";
        case MBR_TYPE_FAT16_ABOVE_32MB: return "FAT16B CHS";
        case MBR_TYPE_NTFS_exFAT_HPFS: return "NTFX/exFAT/HPFS";
        case MBR_TYPE_FAT32_CHS: return "FAT32 CHS";
        case MBR_TYPE_FAT32_LBA: return "FAT32 LBA";
        case MBR_TYPE_FAT16_LBA: return "FAT16 LBA";
        case MBR_TYPE_EXTENDED_PARTITION_LBA: return "Extended Partition LBA";

        case MBR_TYPE_UNKNOWN: return "Unknown";
        case MBR_TYPE_LINUX_SWAP: return "Linux Swap";
        case MBR_TYPE_LINUX: return "Linux";
        case MBR_TYPE_LINUX_EXTENDED: return "Linux extended";
        case MBR_TYPE_LINUX_LVM: return "Linux LVM";

        case MBR_TYPE_FREEBSD: return "FreeBSD";
        case MBR_TYPE_OPENBSD: return "OpenBSD";

        case MBR_TYPE_APPLE_DARWIN_UFS: return "Apple Darwin UFS";
        
        case MBR_TYPE_NETBSD: return "NetBSD";

        case MBR_TYPE_APPLE_HFS_HFS_PLUS: return "Apple HFS/HFS+";

        case MBR_TYPE_VMWARE_VMFS: return "VMWare VMFS";
        case MBR_TYPE_VMWARE_SWAP: return "VMWare Swap";

        case MBR_TYPE_LINUX_RAID_AUTODETECT: return "Linux RAID Autodetect";

        default:
            return PHX_NULL;
    }
}


static PHX_Partition_Type MBR_GetPartitionType(PHX_u8 type)
{
    if (type == MBR_TYPE_UNKNOWN)
        return PHX_PARTITION_TYPE_UNKNOWN;
    return PHX_PARTITION_TYPE_SPECIFIC;
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


static inline void MBR_UpdatePartitionInfo(MBR_Partition* partition)
{
    partition->partition.flags &= ~PHX_PARTITION_FLAGS_CHANGED;
}


static PHX_Result MBR_WriteChanges(PHX_Partition_Table_Device* table)
{
    const PHX_SectorSize bootsectorSectors = (512 + table->disk->sectorSize - 1) / table->disk->sectorSize;
    MBR_Data* data = table->data;

    if (table->disk->read(table->disk, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
        return PHX_ERROR_IO;

    memcpy(data->bootsectorBuffer, data->bootsectorBuffer, 512);

    PHX_u32 signature = (PHX_u32)(table->signature.first & 0xFFFFFFFF);
    PHX_Write_Le_u32(data->bootsectorBuffer + MBR_BOOTSECTOR_DSIG, signature);

    PHX_Write_Le_u16(data->bootsectorBuffer + MBR_BOOTSECTOR_RES, 0);

    const PHX_u32 sectorsPerCylinder = (PHX_u32)table->disk->totalHeads * (PHX_u32)table->disk->sectorsPerTrack;

    for (int i = 0; i < 4; i++)
    {
        MBR_Partition* partition = &((MBR_Partition*)table->partitions)[i];
        PHX_u8* rawPartition = data->bootsectorBuffer + MBR_BOOTSECTOR_PART + (PHX_Size)i * 16;

        if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
            MBR_UpdatePartitionInfo(partition);

        if (partition->partition.type == PHX_PARTITION_TYPE_EMPTY)
        {
            memset(rawPartition, 0, 16);
            continue;
        }
        
        PHX_u8 bootflag = 0x00;
        if (partition->partition.flags & PHX_PARTITION_FLAGS_LEGACY_BOOTABLE)
            bootflag = 0x80;
        
        const PHX_u32 start = (PHX_u32)partition->partition.start; // TODO
        const PHX_u32 size = (PHX_u32)partition->partition.size; // TODO

        const PHX_SectorSize firstCylinder = partition->partition.start / sectorsPerCylinder;
        const PHX_u8 firstHead = (PHX_u8)((partition->partition.start / (PHX_SectorSize)table->disk->sectorsPerTrack) % (PHX_SectorSize)table->disk->totalHeads);
        const PHX_u8 firstSector = (PHX_u8)((partition->partition.start % (PHX_SectorSize)table->disk->sectorsPerTrack) + 1);

        const PHX_SectorSize lastCylinder = (partition->partition.start + partition->partition.size - 1) / sectorsPerCylinder;
        const PHX_u8 lastHead = (PHX_u8)(((partition->partition.start + partition->partition.size - 1) / (PHX_SectorSize)table->disk->sectorsPerTrack) % (PHX_SectorSize)table->disk->totalHeads);
        const PHX_u8 lastSector = (PHX_u8)(((partition->partition.start + partition->partition.size - 1) % (PHX_SectorSize)table->disk->sectorsPerTrack) + 1);

        
        PHX_Write_Le_u8(rawPartition + MBR_PARTITION_BFL, bootflag);

        if (firstCylinder <= 1023)
            MBR_EncodeCHS((PHX_u16)firstCylinder, firstHead, firstSector, rawPartition + MBR_PARTITION_CHSF);
        else
            MBR_EncodeCHS(1023, 254, 63, rawPartition + MBR_PARTITION_CHSF);

        PHX_Write_Le_u8(rawPartition + MBR_PARTITION_TYP, partition->type);

        if (lastCylinder <= 1023)
            MBR_EncodeCHS((PHX_u16)lastCylinder, lastHead, lastSector, rawPartition + MBR_PARTITION_CHSL);
        else
            MBR_EncodeCHS(1023, 254, 63, rawPartition + MBR_PARTITION_CHSL);

        PHX_Write_Le_u32(rawPartition + MBR_PARTITION_STL, start);
        PHX_Write_Le_u32(rawPartition + MBR_PARTITION_SEC, size);
    }

    PHX_Write_Le_u16(data->bootsectorBuffer + MBR_BOOTSECTOR_SIG, MBR_SIGNATURE);

    if (table->disk->write(table->disk, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
        return PHX_ERROR_IO;

    return PHX_SUCCESS;
}


static PHX_Bool MBR_SetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, const char* typeName)
{
    if (index >= table->partitionSlots) return PHX_FALSE;
    MBR_Partition* partition = &((MBR_Partition*)table->partitions)[index];

    PHX_u8 type = MBR_GetTypeByName(typeName);
    if (type == 0) return PHX_FALSE;

    partition->type = type;
    partition->partition.type = MBR_GetPartitionType(type);

    return PHX_TRUE;
}

static const char* MBR_GetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index)
{
    if (index >= table->partitionSlots) return PHX_NULL;
    MBR_Partition* partition = &((MBR_Partition*)table->partitions)[index];

    if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
        MBR_UpdatePartitionInfo(partition);

    return MBR_GetTypeName(partition->type);
}

static void MBR_GetSpecificType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, char* typeOut)
{
    if (index >= table->partitionSlots)
    {
        memset(typeOut, '-', MBR_SPECIFIC_TYPE_LEN - 1);
        typeOut[MBR_SPECIFIC_TYPE_LEN - 1] = '\0';
        return;
    }

    MBR_Partition* partition = &((MBR_Partition*)table->partitions)[index];

    if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
        MBR_UpdatePartitionInfo(partition);

    typeOut[0] = '0';
    typeOut[1] = 'x';
    
    const PHX_u8 upper = partition->type >> 4;
    if (upper < 10)
        typeOut[2] = '0' + upper;
    else
        typeOut[2] = 'A' + (upper - 10);

    const PHX_u8 lower = partition->type & 0x0F;
    if (lower < 10)
        typeOut[3] = '0' + lower;
    else
        typeOut[3] = 'A' + (lower - 10);

    typeOut[4] = '\0';
}


static void MBR_Close(PHX_Partition_Table_Device* table)
{
    MBR_Data* data = table->data;

    table->context->allocator->free(table->context->allocator, table->partitionBuffer);
    table->context->allocator->free(table->context->allocator, table->partitions);
    MBR_FreeData(table->context, data);
}


static const PHX_Partition_Table_Operations MBR_Operations = {
    MBR_WriteChanges,

    MBR_ChangeBootsector,

    MBR_SetType,

    MBR_GetType,
    MBR_GetSpecificType,

    MBR_Close,

    MBR_TypeInfos,
    sizeof(MBR_TypeInfos) / sizeof(PHX_Partition_Type_Info),

    MBR_SPECIFIC_TYPE_LEN
};


#include <stdio.h>
static PHX_Result MBR_OpenPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable)
{
    const PHX_SectorSize bootsectorSectors = (512 + device->sectorSize - 1) / device->sectorSize;
    if (bootsectorSectors >= device->totalSectors)
        return PHX_ERROR_SIZE;

    MBR_Data* data = MBR_AllocateData(context, bootsectorSectors, device->sectorSize);
    if (!data) return PHX_ERROR_MEMORY;

    if (device->read(device, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
    {
        MBR_FreeData(context, data);
        return PHX_ERROR_IO;
    }

    memcpy(data->bootsector, data->bootsectorBuffer, 512);

    const PHX_u16 signature = PHX_Read_Le_u16(data->bootsector + MBR_BOOTSECTOR_SIG);
    if (signature != MBR_SIGNATURE)
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
        const PHX_u32 sectorCount = PHX_Read_Le_u32(rawPartition + MBR_PARTITION_SEC);

        if (type == MBR_TYPE_EMPTY)
        {
            partition->partition.type = PHX_PARTITION_TYPE_EMPTY;
            continue;
        }

        partition->partition.signature.first = 0;
        partition->partition.signature.second = 0;
        partition->partition.signature.type = PHX_PARTITION_SIGNATURE_NONE;

        partition->partition.start = startLba;
        partition->partition.size = sectorCount;

        partition->partition.flags = 0;
        if (bootFlag == 0x80)
            partition->partition.flags |= PHX_PARTITION_FLAGS_LEGACY_BOOTABLE;

        partition->partition.type = MBR_GetPartitionType(type);

        partition->partition.name[0] = '\0';

        partition->type = type;
    }

    outTable->signature.first = diskSignature;
    outTable->signature.second = 0;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_FIRST;

    outTable->partitionSlots = 4;

    outTable->startUsable = 1;
    outTable->sizeUsable = device->totalSectors - 1;

    outTable->context = context;
    outTable->disk = device;

    outTable->ops = &MBR_Operations;
    outTable->type = MBR_TYPE;

    outTable->data = data;

    outTable->partitionBuffer = partitionBuffer;
    outTable->partitions = (PHX_u8*)partitions;
    outTable->partitionSize = sizeof(MBR_Partition);
    
    return PHX_SUCCESS;
}

static PHX_Result MBR_FormatPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable, const PHX_u8* bootsector)
{
    const PHX_SectorSize bootsectorSectors = (512 + device->sectorSize - 1) / device->sectorSize;
    if (bootsectorSectors >= device->totalSectors)
        return PHX_ERROR_SIZE;

    MBR_Data* data = MBR_AllocateData(context, bootsectorSectors, device->sectorSize);
    if (!data) return PHX_ERROR_MEMORY;

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

    if (bootsector)
        memcpy(data->bootsector, bootsector, 512);
    else
        memcpy(data->bootsector, binary_file_data, 512);

    for (int i = 0; i < 4; i++)
    {
        MBR_Partition* partition = &partitions[i];

        partition->partition.signature.first = 0;
        partition->partition.signature.second = 0;
        partition->partition.signature.type = PHX_PARTITION_SIGNATURE_NONE;

        partition->partition.start = 0;
        partition->partition.size = 0;

        partition->partition.flags = 0;
        partition->partition.type = PHX_PARTITION_TYPE_EMPTY;

        partition->type = 0;
    }

    outTable->signature.first = 0;
    while (outTable->signature.first == 0) outTable->signature.first = PHX_Context_GetRandomU32(context);
    outTable->signature.second = 0;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_FIRST;

    outTable->partitionSlots = 4;

    outTable->startUsable = 1;
    outTable->sizeUsable = device->totalSectors - 1;

    outTable->context = context;
    outTable->disk = device;

    outTable->ops = &MBR_Operations;
    outTable->type = MBR_TYPE;

    outTable->data = data;

    outTable->partitionBuffer = partitionBuffer;
    outTable->partitions = (PHX_u8*)partitions;
    outTable->partitionSize = sizeof(MBR_Partition);

    return PHX_SUCCESS;
}


PHX_Partition_Interface PHX_Partition_MBR_Interface = {
    MBR_OpenPartitionTable,
    MBR_FormatPartitionTable,
    &MBR_Operations,
    MBR_TYPE,
    "MBR-PARTITION-TABLE-INTERFACE",
    "mbr"
};
