#include "gpt.h"
#include "phx/device/partition.h"

#include <phx/base/serialization.h>
#include <phx/base/intrinsic.h>
#include <phx/charsets/unicode.h>

#include <embed/gpt.h>

/*

    Protective MBR:
        Bootcode (440 bytes)
        Disk signature (4 bytes)
        reserved (2 bytes)
        entry (16 bytes)
        other 3 empty entries (48 bytes)
        signature (2 bytes)

    GPT Header:
        signature "EFI PART" (8 bytes)
        revision (4 bytes)
        header-size (4 bytes)
        crc32 (4 bytes)
        reserved (4 bytes)
        lba this header (8 bytes)
        lba other header (8 bytes)
        first usable lba (8 bytes)
        last usable lba (8 bytes)
        disk guid (16 bytes)
        start lba partition entry array (8 bytes)
        entry count (4 bytes)
        size of entry (4 bytes)
        crc32 partition entry array (4 bytes)

    Partition Entry:
        partition type guid (16 bytes)
        partition guid (16 bytes)
        first lba (8 bytes)
        last lba (8 bytes)
        attribute flags (8 bytes)
        name (72 bytes, UTF-16LE)

    GUID:
        data1 (4 bytes, little endian)
        data2 (2 bytes, little endian)
        data3 (2 bytes, little endian)
        data4 (8 bytes, big endian)

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

#define MBR_TYPE_PROTECTED 0xEE


#define PHX_GPT_HEADER_SIG 0
#define PHX_GPT_HEADER_REV 8
#define PHX_GPT_HEADER_HES 12
#define PHX_GPT_HEADER_CRC 16
#define PHX_GPT_HEADER_RES 20
#define PHX_GPT_HEADER_LBA 24
#define PHX_GPT_HEADER_ALTLBA 32
#define PHX_GPT_HEADER_FUL 40
#define PHX_GPT_HEADER_LUL 48
#define PHX_GPT_HEADER_DGUID 56
#define PHX_GPT_HEADER_SPA 72
#define PHX_GPT_HEADER_PEC 80
#define PHX_GPT_HEADER_PES 84
#define PHX_GPT_HEADER_CRCPEA 88

#define PHX_GPT_PENTRY_TYP 0
#define PHX_GPT_PENTRY_PGU 16
#define PHX_GPT_PENTRY_FIL 32
#define PHX_GPT_PENTRY_LAL 40
#define PHX_GPT_PENTRY_ATR 48
#define PHX_GPT_PENTRY_NAM 56

#define PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION   (PHX_u64)(1 << 0)
#define PHX_GPT_ATTRIBUTE_NO_BLOCK_IO_PROTOCOL (PHX_u64)(1 << 1)
#define PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE      (PHX_u64)(1 << 2)


#define GPT_TYPE_ESP \
    0xC12A7328, 0xF81F, 0x11D2, 0xBA4B, 0x00A0C93EC93B

#define GPT_TYPE_MICROSOFT_BASIC_DATA \
    0xEBD0A0A2, 0xB9E5, 0x4433, 0x87C0, 0x68B6B72699C7

#define GPT_TYPE_LINUX_FILESYSTEM_DATA \
    0x0Fc63DAF, 0x8483, 0x4772, 0x8E79, 0x3D69D8477DE4

#define GPT_TYPE_APPLE_APFS_CONTAINER \
    0x7C3457EF, 0x0000, 0x11AA, 0xAA11, 0x00306543ECAC


typedef struct
{
    PHX_u64 first;
    PHX_u64 second;
} GUID;

static inline GUID GUID_Create(PHX_u32 d1, PHX_u16 d2, PHX_u16 d3, PHX_u16 d4, PHX_u64 d5)
{
    GUID guid;
    guid.second = ((PHX_u64)d1 << 32) | ((PHX_u64)d2 << 16) | (PHX_u64)d3;
    guid.first = ((PHX_u64)d4 << 48) | (PHX_u64)d5;
    return guid;
}

static inline int GUID_Compare(GUID guid, PHX_u32 d1, PHX_u16 d2, PHX_u16 d3, PHX_u16 d4, PHX_u64 d5)
{
    GUID other = GUID_Create(d1, d2, d3, d4, d5);

    return guid.first == other.first && guid.second == other.second;
}


#define GPT_SPECIFIC_TYPE_LEN 37
#define GPT_TYPE "GPT-PARTITION-TABLE"


typedef struct
{
    PHX_Partition partition;
    GUID type;
    PHX_u64 attributes;
} GPT_Partition;


typedef struct
{
    PHX_SectorSize partitionArrayLba; // 0 if none
    PHX_SectorSize altPartitionArrayLba; // 0 if none

    PHX_u8* bootsectorBuffer;
    PHX_u8* gptHeaderBuffer;
    PHX_u8* partitionArrayBuffer;

    PHX_u32 partitionEntrySize;

    PHX_u8 bootsector[512];
} GPT_Data;


static PHX_Partition_Type_Info GPT_TypeInfos[] = {
    {
        PHX_PARTITION_TYPE_INFO_NAMES("unknown")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("efi", "esp")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("microsoft-basic-data", "ms", "ms-basic-data")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("linux-filesystem-data", "linux-fs-data", "linux-data")
    },
    {
        PHX_PARTITION_TYPE_INFO_NAMES("apple-filesystem-container", "apfs-container", "apfs")
    }
};

static GUID GPT_GetTypeByName(const char* name)
{
    if (strcmp(name, "efi") == 0 || strcmp(name, "esp") == 0)
        return GUID_Create(GPT_TYPE_ESP);

    if (strcmp(name, "microsoft-basic-data") == 0 || strcmp(name, "ms") == 0 || strcmp(name, "ms-basic-data") == 0)
        return GUID_Create(GPT_TYPE_MICROSOFT_BASIC_DATA);

    if (strcmp(name, "linux-filesystem-data") == 0 || strcmp(name, "linux-fs-data") == 0 || strcmp(name, "linux-data") == 0)
        return GUID_Create(GPT_TYPE_LINUX_FILESYSTEM_DATA);

    if (strcmp(name, "apple-filesystem-container") == 0 || strcmp(name, "apfs-container") == 0 || strcmp(name, "apfs") == 0)
        return GUID_Create(GPT_TYPE_APPLE_APFS_CONTAINER);

    return GUID_Create(GPT_TYPE_LINUX_FILESYSTEM_DATA);
}


static const char* GPT_GetTypeName(GUID type)
{
    if (GUID_Compare(type, GPT_TYPE_ESP))
        return "EFI System Partition";

    if (GUID_Compare(type, GPT_TYPE_MICROSOFT_BASIC_DATA))
        return "Microsoft Basic Data";

    if (GUID_Compare(type, GPT_TYPE_LINUX_FILESYSTEM_DATA))
        return "Linux Filesystem Data";

    if (GUID_Compare(type, GPT_TYPE_APPLE_APFS_CONTAINER))
        return "Apple APFS Container";

    return PHX_NULL;
}


static PHX_Partition_Type GPT_GetPartitionType(GUID type)
{
    // TODO
    (void)type;
    return PHX_PARTITION_TYPE_SPECIFIC;
}


static PHX_u32 crc32_calculate(const void* data, PHX_Size size)
{
    PHX_u32 table[256];
    for (PHX_u32 index = 0; index < 256; index++)
    {
        PHX_u32 remainder = index;
        for (int bit = 0; bit < 8; bit++)
            remainder = (remainder & 1u) ? ((remainder >> 1) ^ 0xEDB88320u) : (remainder >> 1);

        table[index] = remainder;
    }

    const PHX_u8* bytes = (const PHX_u8*)data;
    PHX_u32 crc = 0xFFFFFFFFu;
    for (PHX_Size i = 0; i < size; i++)
        crc = table[(crc ^ bytes[i]) & 0xFFu] ^ (crc >> 8);

    return ~crc;
}


static inline GUID read_guid(const PHX_u8* pointer)
{
    GUID guid;

    PHX_u32 data1 = PHX_Read_Le_u32(pointer);
    PHX_u16 data2 = PHX_Read_Le_u16(pointer + 4);
    PHX_u16 data3 = PHX_Read_Le_u16(pointer + 6);
    PHX_u64 data4 = PHX_Read_Be_u64(pointer + 8);

    guid.first = data4;
    guid.second = (PHX_u64)data3 | ((PHX_u64)data2 << 16) | ((PHX_u64)data1 << 32);

    return guid;
}

static inline void write_guid(PHX_u8* pointer, GUID guid)
{
    PHX_Write_Le_u32(pointer, (PHX_u32)(guid.second >> 32));
    PHX_Write_Le_u16(pointer + 4, (PHX_u16)(guid.second >> 16));
    PHX_Write_Le_u16(pointer + 6, (PHX_u16)guid.second);
    PHX_Write_Be_u64(pointer + 8, guid.first);
}


static GPT_Data* GPT_AllocateData(PHX_Context* context, PHX_SectorSize bootsectorSectors, PHX_SectorSize gptHeaderSectors, PHX_SectorSize partitionArraySectors, PHX_SectorSize sectorSize)
{
    GPT_Data* data = context->allocator->allocate(context->allocator, sizeof(GPT_Data) + (bootsectorSectors + gptHeaderSectors + partitionArraySectors) * sectorSize);
    if (!data) return PHX_NULL;

    PHX_u8* other = (PHX_u8*)(data + 1);

    data->bootsectorBuffer = other;
    data->gptHeaderBuffer = other + bootsectorSectors * sectorSize;
    data->partitionArrayBuffer = other + bootsectorSectors * sectorSize * 2;

    memset(data->bootsectorBuffer, 0, bootsectorSectors * sectorSize);
    memset(data->gptHeaderBuffer, 0, gptHeaderSectors * sectorSize);
    memset(data->partitionArrayBuffer, 0, partitionArraySectors * sectorSize);

    return data;
}

static GPT_Data* GPT_ReallocateData(PHX_Context* context, GPT_Data* data, PHX_SectorSize bootsectorSectors, PHX_SectorSize gptHeaderSectors, PHX_SectorSize newPartitionArraySectors, PHX_SectorSize sectorSize)
{
    GPT_Data* newData = context->allocator->reallocate(context->allocator, data, sizeof(GPT_Data) + (bootsectorSectors + gptHeaderSectors + newPartitionArraySectors) * sectorSize);
    if (!newData) return PHX_NULL;

    PHX_u8* other = (PHX_u8*)(newData + 1);

    newData->bootsectorBuffer = other;
    newData->gptHeaderBuffer = other + bootsectorSectors * sectorSize;
    newData->partitionArrayBuffer = other + bootsectorSectors * sectorSize * 2;

    return newData;
}

static void GPT_FreeData(PHX_Context* context, GPT_Data* data)
{
    context->allocator->free(context->allocator, data);
}


static void GPT_UpdatePartitionInfo(GPT_Partition* partition)
{
    if (partition->type.first == 0 && partition->type.second == 0)
        partition->type = GUID_Create(GPT_TYPE_LINUX_FILESYSTEM_DATA); // TODO: Think about it

    if ((partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED) == 0)
        return;

    if (partition->partition.flags & PHX_PARTITION_FLAGS_REQUIRED)
        partition->attributes |= PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION;
    else
        partition->attributes &= ~PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION;

    if (partition->partition.flags & PHX_PARTITION_FLAGS_LEGACY_BOOTABLE)
        partition->attributes |= PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE;
    else
        partition->attributes &= ~PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE;

    partition->partition.flags &= ~PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE;
}


static inline void GPT_EncodeCHS(PHX_u16 cylinder, PHX_Byte head, PHX_Byte sector, PHX_Byte out[3])
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

static inline void GPT_WriteProtectedMBR(PHX_u8* bootsector, PHX_Disk_Device* disk)
{
    const PHX_u32 sectorsPerCylinder = (PHX_u32)disk->totalHeads * (PHX_u32)disk->sectorsPerTrack;
    PHX_u8* rawPartition = bootsector + MBR_BOOTSECTOR_PART;

    memset(bootsector + MBR_BOOTSECTOR_DSIG, 0, 512 - MBR_BOOTSECTOR_DSIG);

    const PHX_SectorSize start = 1;
    const PHX_SectorSize size = disk->totalSectors - 1;

    const PHX_SectorSize firstCylinder = start / sectorsPerCylinder;
    const PHX_u8 firstHead = (PHX_u8)((start / (PHX_SectorSize)disk->sectorsPerTrack) % (PHX_SectorSize)disk->totalHeads);
    const PHX_u8 firstSector = (PHX_u8)((start % (PHX_SectorSize)disk->sectorsPerTrack) + 1);

    const PHX_SectorSize lastCylinder = (start + size - 1) / sectorsPerCylinder;
    const PHX_u8 lastHead = (PHX_u8)(((start + size - 1) / (PHX_SectorSize)disk->sectorsPerTrack) % (PHX_SectorSize)disk->totalHeads);
    const PHX_u8 lastSector = (PHX_u8)(((start + size - 1) % (PHX_SectorSize)disk->sectorsPerTrack) + 1);

    // bootflag already 0

    if (firstCylinder <= 1023)
        GPT_EncodeCHS((PHX_u16)firstCylinder, firstHead, firstSector, rawPartition + MBR_PARTITION_CHSF);
    else
        GPT_EncodeCHS(1023, 254, 63, rawPartition + MBR_PARTITION_CHSF);

    PHX_Write_Le_u8(rawPartition + MBR_PARTITION_TYP, MBR_TYPE_PROTECTED);

    if (lastCylinder <= 1023)
        GPT_EncodeCHS((PHX_u16)lastCylinder, lastHead, lastSector, rawPartition + MBR_PARTITION_CHSL);
    else
        GPT_EncodeCHS(1023, 254, 63, rawPartition + MBR_PARTITION_CHSL);

    PHX_Write_Le_u32(rawPartition + MBR_PARTITION_STL, (start > 0xFFFFFFFF) ? 0xFFFFFFFF : (PHX_u32)start);
    PHX_Write_Le_u32(rawPartition + MBR_PARTITION_SEC, (size > 0xFFFFFFFF) ? 0xFFFFFFFF : (PHX_u32)size);

    PHX_Write_Le_u16(bootsector + MBR_BOOTSECTOR_SIG, MBR_SIGNATURE);
}

static PHX_Result GPT_WriteChanges(PHX_Partition_Table_Device* table)
{
    const PHX_SectorSize bootsectorSectors = (512 + table->disk->sectorSize - 1) / table->disk->sectorSize;
    GPT_Data* data = table->data;

    if (table->disk->read(table->disk, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
        return PHX_ERROR_IO;

    memcpy(data->bootsectorBuffer, data->bootsectorBuffer, 512);
    GPT_WriteProtectedMBR(data->bootsectorBuffer, table->disk);

    if (table->disk->write(table->disk, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
        return PHX_ERROR_IO;

    const GUID diskGuid = {table->signature.first, table->signature.second};

    PHX_u32 headerSize = PHX_Read_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_HES);
    if (headerSize == 0)
        headerSize = 92;

    PHX_u32 partitionEntrySize = PHX_Read_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_PES);
    if (partitionEntrySize == 0)
        partitionEntrySize = 128;

    const PHX_SectorSize partitionArraySectors = ((PHX_SectorSize)partitionEntrySize * table->partitionSlots) / table->disk->sectorSize;

    const PHX_SectorSize partitionEntryArrayLba = data->partitionArrayLba ? data->partitionArrayLba : 2;
    const PHX_SectorSize altPartitionEntryArrayLba = data->altPartitionArrayLba ? data->altPartitionArrayLba : (table->disk->totalSectors - partitionArraySectors - 1);

    for (PHX_PartitionSize i = 0; i < table->partitionSlots; i++)
    {
        GPT_Partition* partition = &((GPT_Partition*)table->partitions)[i];
        PHX_u8* rawPartition = data->partitionArrayBuffer + i * partitionEntrySize;

        if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
            GPT_UpdatePartitionInfo(partition);

        if (partition->partition.type == PHX_PARTITION_TYPE_EMPTY)
        {
            memset(rawPartition, 0, partitionEntrySize);
            break;
        }

        GUID partitionGuid = {partition->partition.signature.first, partition->partition.signature.second};

        write_guid(rawPartition + PHX_GPT_PENTRY_TYP, partition->type);
        write_guid(rawPartition + PHX_GPT_PENTRY_PGU, partitionGuid);
        PHX_Write_Le_u64(rawPartition + PHX_GPT_PENTRY_FIL, partition->partition.start);
        PHX_Write_Le_u64(rawPartition + PHX_GPT_PENTRY_LAL, partition->partition.start + partition->partition.size - 1);
        PHX_Write_Le_u64(rawPartition + PHX_GPT_PENTRY_ATR, partition->attributes);

        const char* name = partition->partition.name;
        const PHX_Size nameLen = strlen(partition->partition.name);

        // TODO: Validate name

        memset(rawPartition + PHX_GPT_PENTRY_NAM, 0, 72);
        PHX_Size units = PHX_Charsets_Unicode_UTF8_ToUTF16(name, nameLen, rawPartition + PHX_GPT_PENTRY_NAM, 36, PHX_CHARSETS_UNICODE_ENDIANNESS_LITTLE);
        if (units > 36)
        {
            // TODO: Too long
        }
    }

    const PHX_u32 partitionArrayCrc = crc32_calculate(data->partitionArrayBuffer, partitionArraySectors * table->disk->sectorSize);

    for (int i = 0; i < 2; i++)
    {
        const PHX_SectorSize gptHeaderLba = (i == 0) ? 1 : (table->disk->totalSectors - 1);
        const PHX_SectorSize otherGptHeaderLba = (i == 0) ? (table->disk->totalSectors - 1) : 1;
        const PHX_SectorSize partitionArrayLba = (i == 0) ? partitionEntryArrayLba : altPartitionEntryArrayLba;

        memcpy(data->gptHeaderBuffer + PHX_GPT_HEADER_SIG, "EFI PART", 8);

        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_REV, 0x00010000);

        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_HES, headerSize);

        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_CRC, 0);

        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_RES, 0);

        PHX_Write_Le_u64(data->gptHeaderBuffer + PHX_GPT_HEADER_LBA, gptHeaderLba);
        PHX_Write_Le_u64(data->gptHeaderBuffer + PHX_GPT_HEADER_ALTLBA, otherGptHeaderLba);

        PHX_Write_Le_u64(data->gptHeaderBuffer + PHX_GPT_HEADER_FUL, table->startUsable);
        PHX_Write_Le_u64(data->gptHeaderBuffer + PHX_GPT_HEADER_LUL, table->startUsable + table->sizeUsable - 1);

        write_guid(data->gptHeaderBuffer + PHX_GPT_HEADER_DGUID, diskGuid);

        PHX_Write_Le_u64(data->gptHeaderBuffer + PHX_GPT_HEADER_SPA, partitionArrayLba);
        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_PEC, (PHX_u32)table->partitionSlots); // TODO
        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_PES, partitionEntrySize);
        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_CRCPEA, partitionArrayCrc);
        
        PHX_u32 headerCrc = crc32_calculate(data->gptHeaderBuffer, headerSize);
        PHX_Write_Le_u32(data->gptHeaderBuffer + PHX_GPT_HEADER_CRC, headerCrc);

        if (table->disk->write(table->disk, data->gptHeaderBuffer, gptHeaderLba, 1) != 1)
            return PHX_ERROR_IO;

        if (table->disk->write(table->disk, data->partitionArrayBuffer, partitionArrayLba, partitionArraySectors) != partitionArraySectors)
            return PHX_ERROR_IO;
    }

    return PHX_SUCCESS;
}


static PHX_Result GPT_ChangeBootsector(PHX_Partition_Table_Device* table, const PHX_u8* bootsector)
{
    GPT_Data* data = table->data;
    memcpy(data->bootsector, bootsector, 512);
    return PHX_SUCCESS;
}


static PHX_Bool GPT_SetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, const char* typeName)
{
    if (index >= table->partitionSlots) return PHX_FALSE;
    GPT_Partition* partition = &((GPT_Partition*)table->partitions)[index];

    GUID type = GPT_GetTypeByName(typeName);
    if (type.first == 0 && type.second == 0) return PHX_FALSE;

    partition->type = type;
    partition->partition.type = GPT_GetPartitionType(type);

    return PHX_TRUE;
}

static const char* GPT_GetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index)
{
    if (index >= table->partitionSlots) return PHX_NULL;
    GPT_Partition* partition = &((GPT_Partition*)table->partitions)[index];

    if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
        GPT_UpdatePartitionInfo(partition);

    return GPT_GetTypeName(partition->type);
}

static void put_hex(char* p, PHX_u64 value, int digits)
{
    static const char hex[] = "0123456789ABCDEF";

    for (int i = digits - 1; i >= 0; i--) {
        p[i] = hex[value & 0xF];
        value >>= 4;
    }
}

static void GPT_GetSpecificType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, char* typeOut)
{
    if (index >= table->partitionSlots)
    {
        memset(typeOut, '-', GPT_SPECIFIC_TYPE_LEN - 1);
        typeOut[GPT_SPECIFIC_TYPE_LEN - 1] = '\0';
        return;
    }

    GPT_Partition* partition = &((GPT_Partition*)table->partitions)[index];

    if (partition->partition.flags & PHX_PARTITION_FLAGS_CHANGED)
        GPT_UpdatePartitionInfo(partition);

    const PHX_u64 first = partition->type.first;
    const PHX_u64 second = partition->type.second;

    const PHX_u32 v1 = (PHX_u32)(second >> 32);
    const PHX_u16 v2 = (PHX_u16)((second >> 16) & 0xFFFF);
    const PHX_u16 v3 = (PHX_u16)(second & 0xFFFF);
    const PHX_u16 v4 = (PHX_u16)(first >> 48);
    const PHX_u64 v5 = first & 0xFFFFFFFFFFFF;

    put_hex(typeOut, v1, 8);
    typeOut[8] = '-';

    put_hex(typeOut + 9, v2, 4);
    typeOut[13] = '-';

    put_hex(typeOut + 14, v3, 4);
    typeOut[18] = '-';

    put_hex(typeOut + 19, v4, 4);
    typeOut[23] = '-';

    put_hex(typeOut + 24, v5, 12);
    typeOut[36] = '\0';
}


static void GPT_Close(PHX_Partition_Table_Device* table)
{
    GPT_Data* data = table->data;

    table->context->allocator->free(table->context->allocator, table->partitionBuffer);
    table->context->allocator->free(table->context->allocator, table->partitions);
    GPT_FreeData(table->context, data);
}


static const PHX_Partition_Table_Operations GPT_Operations = {
    GPT_WriteChanges,

    GPT_ChangeBootsector,

    GPT_SetType,

    GPT_GetType,
    GPT_GetSpecificType,

    GPT_Close,

    GPT_TypeInfos,
    sizeof(GPT_TypeInfos) / sizeof(PHX_Partition_Type_Info),

    GPT_SPECIFIC_TYPE_LEN
};


static int isPowerOfTwo(PHX_u32 v) { return v && !(v & (v - 1)); }

static PHX_Result GPT_OpenPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable)
{
    if (device->sectorSize < 92)
        return PHX_ERROR_SIZE; // TODO

    const PHX_SectorSize bootsectorSectors = (512 + device->sectorSize - 1) / device->sectorSize;
    if (bootsectorSectors + 2 >= device->totalSectors)
        return PHX_ERROR_SIZE;

    GPT_Data* data = GPT_AllocateData(context, bootsectorSectors, 1, 0, device->sectorSize);
    if (!data) return PHX_ERROR_MEMORY;

    if (device->read(device, data->bootsectorBuffer, 0, bootsectorSectors) != bootsectorSectors)
    {
        GPT_FreeData(context, data);
        return PHX_ERROR_IO;
    }

    memcpy(data->bootsector, data->bootsectorBuffer, 512);

    const PHX_u16 signature = PHX_Read_Le_u16(data->bootsector + MBR_BOOTSECTOR_SIG);
    if (signature != MBR_SIGNATURE)
    {
        GPT_FreeData(context, data);
        return PHX_ERROR_FORMAT;
    }

    PHX_Bool found = PHX_FALSE;
    for (int i = 0; i < 4; i++)
    {
        PHX_u8* partition = data->bootsector + MBR_BOOTSECTOR_PART + (PHX_Size)i * 16;

        const PHX_u8 type = PHX_Read_Le_u8(partition + MBR_PARTITION_TYP);

        PHX_u32 startLBA;
        memcpy(&startLBA, partition + 8, 4);
        startLBA = Endian_Convert_u32_Le(startLBA);

        if (type == MBR_TYPE_PROTECTED && startLBA == 1)
        {
            found = PHX_TRUE;
            break;
        }
    }

    if (found != PHX_TRUE)
    {
        GPT_FreeData(context, data);
        return PHX_FALSE;
    }

    PHX_u64 firstUsableLba;
    PHX_u64 lastUsableLba;

    PHX_u32 partitionEntryCount;
    PHX_u32 partitionEntrySize;

    PHX_u8* altGptHeaderBuffer = PHX_NULL;
    PHX_Result result = PHX_ERROR_FORMAT;
    for (int i = 0; i < 2; i++)
    {
        const PHX_SectorSize gptHeaderLba = (i == 0) ? 1 : (device->totalSectors - 1);
        if (i == 0)
            data->partitionArrayLba = 0;
        else
            data->altPartitionArrayLba = 0;

        PHX_u8* gptHeaderBuffer;
        if (result == PHX_SUCCESS)
        {
            if (!altGptHeaderBuffer)
                altGptHeaderBuffer = context->allocator->allocate(context->allocator, device->sectorSize);
            if (!altGptHeaderBuffer)
            {
                if (result == PHX_SUCCESS) continue;
                result = PHX_ERROR_MEMORY;
                continue;
            }

            gptHeaderBuffer = altGptHeaderBuffer;
        }
        else
            gptHeaderBuffer = data->gptHeaderBuffer;

        if (device->read(device, gptHeaderBuffer, gptHeaderLba, 1) != 1)
        {
            if (result == PHX_SUCCESS) continue;
            result = PHX_ERROR_IO;
            continue;
        }

        const unsigned char* gptSignature = gptHeaderBuffer + PHX_GPT_HEADER_SIG;
        if (memcmp(gptSignature, "EFI PART", 8) != 0)
            continue;

        const PHX_u32 revision = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_REV);
        if (revision != 0x00010000)
            continue;

        const PHX_u32 headerSize = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_HES);
        if (headerSize < 92 || headerSize > device->sectorSize)
            continue;

        const PHX_u32 headerCrc = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_CRC);
        PHX_Write_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_CRC, 0);

        if (headerCrc != crc32_calculate(gptHeaderBuffer, headerSize))
            continue;

        const PHX_u64 thisHeaderLba = PHX_Read_Le_u64(gptHeaderBuffer + PHX_GPT_HEADER_LBA);
        if (thisHeaderLba != gptHeaderLba)
            continue;

        const PHX_u64 firstUsableLbaI = PHX_Read_Le_u64(gptHeaderBuffer + PHX_GPT_HEADER_FUL);
        const PHX_u64 lastUsableLbaI = PHX_Read_Le_u64(gptHeaderBuffer + PHX_GPT_HEADER_LUL);

        if (firstUsableLbaI > lastUsableLbaI)
            continue;

        const PHX_u64 startLbaPartitionEntryArray = PHX_Read_Le_u64(gptHeaderBuffer + PHX_GPT_HEADER_SPA);
        const PHX_u32 partitionEntryCountI = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_PEC);
        const PHX_u32 partitionEntrySizeI = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_PES);
        PHX_u32 crcPartitionEntryArray = PHX_Read_Le_u32(gptHeaderBuffer + PHX_GPT_HEADER_CRCPEA);

        PHX_u64 partitionEntryArraySectors = ((PHX_u64)partitionEntryCountI * (PHX_u64)partitionEntrySizeI + device->sectorSize - 1) / device->sectorSize;

        if (partitionEntrySizeI % 128 != 0 || !isPowerOfTwo(partitionEntrySizeI / 128))
            continue;

        if (startLbaPartitionEntryArray > device->totalSectors || partitionEntryArraySectors > device->totalSectors - startLbaPartitionEntryArray)
            continue;

        int beforeUsable = startLbaPartitionEntryArray + partitionEntryArraySectors <= firstUsableLbaI;
        int afterUsable  = startLbaPartitionEntryArray > lastUsableLbaI;
        if (!beforeUsable && !afterUsable)
            continue;

        if (lastUsableLbaI >= device->totalSectors - 1 - partitionEntryArraySectors)
            continue;

        GPT_Data* newData = GPT_ReallocateData(context, data, bootsectorSectors, 1, partitionEntryArraySectors, device->sectorSize);
        if (!newData)
        {
            if (result == PHX_SUCCESS) continue;
            result = PHX_ERROR_MEMORY;
            break;
        }
        data = newData;

        if (device->read(device, newData->partitionArrayBuffer, startLbaPartitionEntryArray, partitionEntryArraySectors) != partitionEntryArraySectors)
        {
            if (result == PHX_SUCCESS) continue;
            result = PHX_ERROR_IO;
            continue;
        }

        if (crcPartitionEntryArray != crc32_calculate(data->partitionArrayBuffer, partitionEntryArraySectors * device->sectorSize))
            continue;

        PHX_Bool allValid = PHX_TRUE;
        for (PHX_u32 index = 0; index < partitionEntryCountI; index++)
        {
            const PHX_u8* partitionEntry = data->partitionArrayBuffer + index * partitionEntrySizeI;

            GUID typeGuid = read_guid(partitionEntry + PHX_GPT_PENTRY_TYP);
            if (typeGuid.first == 0 && typeGuid.second == 0)
                continue;

            const PHX_u64 partitionFirstLba = PHX_Read_Le_u64(partitionEntry + PHX_GPT_PENTRY_FIL);
            const PHX_u64 partitionLastLba = PHX_Read_Le_u64(partitionEntry + PHX_GPT_PENTRY_LAL);

            if (partitionFirstLba > partitionLastLba)
            {
                allValid = PHX_FALSE;
                break;
            }
        }

        if (allValid != PHX_TRUE)
            continue;

        if (i == 0)
            data->partitionArrayLba = startLbaPartitionEntryArray;
        else
            data->altPartitionArrayLba = startLbaPartitionEntryArray;

        if (result == PHX_SUCCESS)
            continue;

        firstUsableLba = firstUsableLbaI;
        lastUsableLba = lastUsableLbaI;

        partitionEntryCount = partitionEntryCountI;
        partitionEntrySize = partitionEntrySizeI;

        result = PHX_SUCCESS;
        continue;
    }

    if (altGptHeaderBuffer)
        context->allocator->free(context->allocator, altGptHeaderBuffer);

    if (result != PHX_SUCCESS)
    {
        GPT_FreeData(context, data);
        return result;
    }

    GPT_Partition* partitions = context->allocator->allocate(context->allocator, sizeof(GPT_Partition) * partitionEntryCount);
    if (!partitions)
    {
        GPT_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    PHX_u8* partitionBuffer = context->allocator->allocate(context->allocator, sizeof(GPT_Partition));
    if (!partitionBuffer)
    {
        context->allocator->free(context->allocator, partitions);
        GPT_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    for (PHX_u32 i = 0; i < partitionEntryCount; i++)
    {
        GPT_Partition* partition = &partitions[i];
        PHX_u8* rawPartition = data->partitionArrayBuffer + (PHX_Size)i * partitionEntrySize;

        const GUID partitionTypeGuid = read_guid(rawPartition + PHX_GPT_PENTRY_TYP);
        const GUID partitionGuid = read_guid(rawPartition + PHX_GPT_PENTRY_PGU);
        const PHX_u64 partitionFirstLba = PHX_Read_Le_u64(rawPartition + PHX_GPT_PENTRY_FIL);
        const PHX_u64 partitionLastLba = PHX_Read_Le_u64(rawPartition + PHX_GPT_PENTRY_LAL);
        const PHX_u64 partitionAttributes = PHX_Read_Le_u64(rawPartition + PHX_GPT_PENTRY_ATR);
        const PHX_u8* name16 = rawPartition + PHX_GPT_PENTRY_NAM;

        if (partitionTypeGuid.first == 0 && partitionTypeGuid.second == 0)
        {
            partition->partition.type = PHX_PARTITION_TYPE_EMPTY;
            continue;
        }

        partition->partition.signature.first = partitionGuid.first;
        partition->partition.signature.second = partitionGuid.second;
        partition->partition.signature.type = PHX_PARTITION_SIGNATURE_GUID;

        partition->partition.start = partitionFirstLba;
        partition->partition.size = partitionLastLba - partitionFirstLba + 1;

        partition->partition.flags = 0;
        if (partitionAttributes & PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION)
            partition->partition.flags |= PHX_PARTITION_FLAGS_REQUIRED;
        if (partitionAttributes & PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE)
            partition->partition.flags |= PHX_PARTITION_FLAGS_LEGACY_BOOTABLE;

        partition->partition.type = GPT_GetPartitionType(partitionTypeGuid);

        // Max: 36 * 3 = 108 UTF 8 bytes
        const PHX_Size name16Len = PHX_Charsets_Unicode_UTF16_Length(name16, 36);

        memset(partition->partition.name, 0, sizeof(partition->partition.name));
        PHX_Size nameLen = PHX_Charsets_Unicode_UTF16_ToUTF8(name16, name16Len, partition->partition.name, sizeof(partition->partition.name) - 1, PHX_CHARSETS_UNICODE_ENDIANNESS_LITTLE);

        if (nameLen > sizeof(partition->partition.name) - 1)
        {
            // TODO: Could not read everything
            context->allocator->free(context->allocator, partitionBuffer);
            context->allocator->free(context->allocator, partitions);
            GPT_FreeData(context, data);
            return PHX_ERROR_INTERNAL;
        }

        partition->type = partitionTypeGuid;
        partition->attributes = partitionAttributes;
    }

    GUID diskGuid = read_guid(data->gptHeaderBuffer + PHX_GPT_HEADER_DGUID);

    outTable->signature.first = diskGuid.first;
    outTable->signature.second = diskGuid.second;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_GUID;

    outTable->partitionSlots = partitionEntryCount;

    outTable->startUsable = firstUsableLba;
    outTable->sizeUsable = lastUsableLba - firstUsableLba + 1;

    outTable->context = context;
    outTable->disk = device;

    outTable->ops = &GPT_Operations;
    outTable->type = GPT_TYPE;

    outTable->data = data;

    outTable->partitionBuffer = partitionBuffer;
    outTable->partitions = (PHX_u8*)partitions;
    outTable->partitionSize = sizeof(GPT_Partition);

    return PHX_SUCCESS;
}

static PHX_Result GPT_FormatPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable, const PHX_u8* bootsector)
{
    if (device->sectorSize < 92)
        return PHX_ERROR_SIZE; // TODO

    const PHX_SectorSize bootsectorSectors = (512 + device->sectorSize - 1) / device->sectorSize;

    const PHX_u32 partitionEntrySize = 128;
    const PHX_u32 partitionEntryCount = 128;

    const PHX_SectorSize partitionArraySectors = ((PHX_SectorSize)partitionEntrySize * (PHX_SectorSize)partitionEntryCount + device->sectorSize - 1) / device->sectorSize;

    if (device->totalSectors < bootsectorSectors + 2 + partitionArraySectors * 2 + 1)
        return PHX_ERROR_SIZE;

    GPT_Data* data = GPT_AllocateData(context, bootsectorSectors, 1, partitionArraySectors, device->sectorSize);
    if (!data) return PHX_ERROR_MEMORY;

    if (bootsector)
        memcpy(data->bootsector, bootsector, 512);
    else
        memcpy(data->bootsector, binary_file_data, 512);

    GPT_Partition* partitions = context->allocator->allocate(context->allocator, sizeof(GPT_Partition) * partitionEntryCount);
    if (!partitions)
    {
        GPT_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    PHX_u8* partitionBuffer = context->allocator->allocate(context->allocator, sizeof(GPT_Partition));
    if (!partitionBuffer)
    {
        context->allocator->free(context->allocator, partitions);
        GPT_FreeData(context, data);
        return PHX_ERROR_MEMORY;
    }

    data->partitionArrayLba = 2;
    data->altPartitionArrayLba = device->totalSectors - 1 - partitionArraySectors;

    for (PHX_u32 i = 0; i < partitionEntryCount; i++)
    {
        GPT_Partition* partition = &partitions[i];

        partition->partition.signature.first = 0;
        partition->partition.signature.second = 0;
        partition->partition.signature.type = PHX_PARTITION_SIGNATURE_NONE;

        partition->partition.start = 0;
        partition->partition.size = 0;

        partition->partition.flags = 0;
        partition->partition.type = PHX_PARTITION_TYPE_EMPTY;


        memset(&partition->type, 0, sizeof(partition->type));
        partition->attributes = 0;
    }

    outTable->signature.first = PHX_Context_GetRandomU64(context);
    outTable->signature.second = PHX_Context_GetRandomU64(context);
    outTable->signature.type = PHX_PARTITION_SIGNATURE_GUID;

    outTable->partitionSlots = partitionEntryCount;

    outTable->startUsable = bootsectorSectors + 1 + partitionArraySectors;
    outTable->sizeUsable = device->totalSectors - bootsectorSectors - 2 * (1 + partitionArraySectors);

    outTable->context = context;
    outTable->disk = device;

    outTable->ops = &GPT_Operations;
    outTable->type = GPT_TYPE;

    outTable->data = data;

    outTable->partitionBuffer = partitionBuffer;
    outTable->partitions = (PHX_u8*)partitions;
    outTable->partitionSize = sizeof(GPT_Partition);

    return PHX_SUCCESS;
}


PHX_Partition_Interface PHX_Partition_GPT_Interface = {
    GPT_OpenPartitionTable,
    GPT_FormatPartitionTable,
    &GPT_Operations,
    GPT_TYPE,
    "GPT-PARTITION-TABLE-INTERFACE",
    "gpt"
};
