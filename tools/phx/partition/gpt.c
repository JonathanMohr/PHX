#include "gpt.h"
#include "phx/device/partition.h"
#include "phx/result.h"
#include "phx/types.h"

#include <phx/base/endianness.h>
#include <phx/base/intrinsic.h>

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

typedef struct
{
    PHX_u64 first;
    PHX_u64 second;
} GUID;

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

#define PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION   (1 << 0)
#define PHX_GPT_ATTRIBUTE_NO_BLOCK_IO_PROTOCOL (1 << 1)
#define PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE      (1 << 2)


#define GPT_SPECIFIC_TYPE_LEN 36
#define GPT_TYPE "GPT-PARTITION-TABLE"


static PHX_Partition_Type_Info GPT_TypeInfos[] = {
    {
        PHX_PARTITION_TYPE_INFO_NAMES("unknown")
    }
};


static PHX_Result GPT_WriteChanges(PHX_Partition_Table_Device* table)
{
    // TODO
    (void)table;
    return PHX_ERROR_INTERNAL;
}


static PHX_Result GPT_ChangeBootsector(PHX_Partition_Table_Device* table, const PHX_u8* bootsector)
{
    // TODO
    (void)table;
    (void)bootsector;
    return PHX_ERROR_INTERNAL;
}


static PHX_Bool GPT_SetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, const char* typeName)
{
    // TODO
    (void)table;
    (void)index;
    (void)typeName;
    return PHX_FALSE;
}

static const char* GPT_GetType(PHX_Partition_Table_Device* table, PHX_PartitionSize index)
{
    // TODO
    (void)table;
    (void)index;
    return PHX_NULL;
}

static void GPT_GetSpecificType(PHX_Partition_Table_Device* table, PHX_PartitionSize index, char* typeOut)
{
    // TODO
    (void)table;
    (void)index;
    (void)typeOut;
}


static void GPT_Close(PHX_Partition_Table_Device* table)
{
    // TODO
    (void)table;
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


static PHX_Result GPT_OpenPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable)
{
    // TODO
    (void)context;
    (void)device;
    (void)outTable;
    return PHX_ERROR_INTERNAL;
}

static PHX_Result GPT_FormatPartitionTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table_Device* outTable, const PHX_u8* bootsector)
{
    // TODO
    (void)context;
    (void)device;
    (void)outTable;
    (void)bootsector;
    return PHX_ERROR_INTERNAL;
}


PHX_Partition_Interface PHX_Partition_GPT_Interface = {
    GPT_OpenPartitionTable,
    GPT_FormatPartitionTable,
    &GPT_Operations,
    GPT_TYPE,
    "GPT-PARTITION-TABLE-INTERFACE",
    "gpt"
};


/*

static inline PHX_u32 read_u32(const PHX_Byte* buffer)
{
    PHX_u32 val;
    memcpy(&val, buffer, sizeof(val));
    return PHX_Endianness_Convert_u32_Le(val);
}

static inline PHX_u64 read_u64(const PHX_Byte* buffer)
{
    PHX_u64 val;
    memcpy(&val, buffer, sizeof(val));
    return PHX_Endianness_Convert_u64_Le(val);
}

static inline void write_u32(PHX_Byte* buffer, PHX_u32 val)
{
    PHX_u32 rawVal = PHX_Endianness_Convert_u32_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}

static inline void write_u64(PHX_Byte* buffer, PHX_u64 val)
{
    PHX_u64 rawVal = PHX_Endianness_Convert_u64_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}


static inline GUID read_guid(const PHX_Byte* buffer)
{
    GUID guid;

    PHX_u32 data1;
    memcpy(&data1, buffer, 4);
    data1 = PHX_Endianness_Convert_u32_Le(data1);

    PHX_u16 data2;
    memcpy(&data2, buffer + 4, 2);
    data2 = PHX_Endianness_Convert_u16_Le(data2);

    PHX_u16 data3;
    memcpy(&data3, buffer + 6, 2);
    data3 = PHX_Endianness_Convert_u16_Le(data3);

    PHX_u64 data4;
    memcpy(&data4, buffer + 8, 8);
    data4 = PHX_Endianness_Convert_u64_Be(data4);

    guid.first = data4;
    guid.second = (PHX_u64)data3 | ((PHX_u64)data2 << 16) | ((PHX_u64)data1 << 32);

    return guid;
}

static inline void write_guid(PHX_Byte* buffer, GUID guid)
{
    PHX_u32 data1 = PHX_Endianness_Convert_u32_Le((PHX_u32)(guid.second >> 32));
    PHX_u16 data2 = PHX_Endianness_Convert_u16_Le((PHX_u16)((guid.second >> 16) & 0xFFFF));
    PHX_u16 data3 = PHX_Endianness_Convert_u16_Le((PHX_u16)(guid.second & 0xFFFF));
    PHX_u64 data4 = PHX_Endianness_Convert_u64_Be(guid.first);

    memcpy(buffer, &data1, 4);
    memcpy(buffer + 4, &data2, 2);
    memcpy(buffer + 6, &data3, 2);
    memcpy(buffer + 8, &data4, 8);
}

static inline int is_guid(GUID guid, PHX_u32 data1, PHX_u16 data2, PHX_u16 data3, PHX_u64 data4)
{
    if (guid.first != data4)
        return 0;
    if (guid.second != ((PHX_u64)data3 | ((PHX_u64)data2 << 16) | ((PHX_u64)data1 << 32)))
        return 0;
    return 1;
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

static int isPowerOfTwo(PHX_u32 v) { return v && !(v & (v - 1)); }

static PHX_Bool GPT_ReadTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table* outTable)
{
    // TODO: Think about it
    if (device->sectorSize < 92)
        return PHX_FALSE;

    PHX_u8 bootsector[512];

    const PHX_Size bootsectorBlocks = (512 + device->sectorSize - 1) / device->sectorSize;
    PHX_u8* bootsectorBuffer = context->allocator->allocate(context->allocator, bootsectorBlocks * device->sectorSize);
    if (!bootsectorBuffer)
        return PHX_FALSE;

    // Protective MBR
    if (device->read(device, bootsectorBuffer, 0, bootsectorBlocks) != bootsectorBlocks)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    const PHX_u8 signature[2] = {bootsectorBuffer[510], bootsectorBuffer[511]};
    if (signature[0] != 0x55 || signature[1] != 0xAA)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    PHX_Bool found = PHX_FALSE;
    for (int i = 0; i < 4; i++)
    {
        PHX_u8* partition = bootsectorBuffer + 446 + (PHX_Size)i * 16;

        const PHX_u8 type = partition[4];

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
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    memcpy(bootsector, bootsectorBuffer, 512);

    // GPT Header
    PHX_PartitionSize partitionCount;

    PHX_u64 firstUsableLba;
    PHX_u64 lastUsableLba;

    PHX_u64 startLbaPartitionEntryArray;
    PHX_u32 partitionEntryCount;
    PHX_u32 partitionEntrySize;
    PHX_u32 crcPartitionEntryArray;
    PHX_u64 partitionEntryArraySectors;
    PHX_u8* partitionEntryArrayBuffer = PHX_NULL;

    PHX_Bool foundHeader = PHX_FALSE;
    for (int i = 0; i < 2; i++)
    {
        const PHX_SectorSize gptHeaderLba = (i == 0) ? 1 : (device->totalSectors - 1);

        if (device->read(device, bootsectorBuffer, gptHeaderLba, 1) != bootsectorBlocks)
            continue;

        const unsigned char* sig = bootsectorBuffer + PHX_GPT_HEADER_SIG;
        if (memcmp(sig, "EFI PART", 8) != 0)
            continue;

        const PHX_u32 revision = read_u32(bootsectorBuffer + PHX_GPT_HEADER_REV);
        if (revision != 0x00010000)
            continue;

        const PHX_u32 headerSize = read_u32(bootsectorBuffer + PHX_GPT_HEADER_HES);
        if (headerSize < 92 || headerSize > device->sectorSize)
            continue;

        const PHX_u32 headerCrc = read_u32(bootsectorBuffer + PHX_GPT_HEADER_CRC);
        write_u32(bootsectorBuffer + PHX_GPT_HEADER_CRC, 0);

        if (headerCrc != crc32_calculate(bootsectorBuffer, headerSize))
            continue;

        const PHX_u64 thisHeaderLba = read_u64(bootsectorBuffer + PHX_GPT_HEADER_LBA);
        if (thisHeaderLba != gptHeaderLba)
            continue;

        firstUsableLba = read_u64(bootsectorBuffer + PHX_GPT_HEADER_FUL);
        lastUsableLba = read_u64(bootsectorBuffer + PHX_GPT_HEADER_LUL);

        if (firstUsableLba > lastUsableLba)
            continue;

        startLbaPartitionEntryArray = read_u64(bootsectorBuffer + PHX_GPT_HEADER_SPA);
        partitionEntryCount = read_u32(bootsectorBuffer + PHX_GPT_HEADER_PEC);
        partitionEntrySize = read_u32(bootsectorBuffer + PHX_GPT_HEADER_PES);
        crcPartitionEntryArray = read_u32(bootsectorBuffer + PHX_GPT_HEADER_CRCPEA);

        partitionEntryArraySectors = ((PHX_u64)partitionEntryCount * (PHX_u64)partitionEntrySize + device->sectorSize - 1) / device->sectorSize;

        if (partitionEntrySize % 128 != 0 || !isPowerOfTwo(partitionEntrySize / 128))
            continue;

        if (firstUsableLba < partitionEntryArraySectors)
            continue;

        if (startLbaPartitionEntryArray > firstUsableLba - partitionEntryArraySectors)
            continue;

        if (lastUsableLba >= device->totalSectors - 1 - partitionEntryArraySectors)
            continue;

        partitionEntryArrayBuffer = context->allocator->allocate(context->allocator, partitionEntryArraySectors * device->sectorSize);
        if (!partitionEntryArrayBuffer)
            continue;

        if (device->read(device, partitionEntryArrayBuffer, startLbaPartitionEntryArray, partitionEntryArraySectors) != partitionEntryArraySectors)
        {
            context->allocator->free(context->allocator, partitionEntryArrayBuffer);
            continue;
        }

        if (crcPartitionEntryArray != crc32_calculate(partitionEntryArrayBuffer, partitionEntryArraySectors * device->sectorSize))
        {
            context->allocator->free(context->allocator, partitionEntryArrayBuffer);
            continue;
        }

        partitionCount = 0;
        PHX_Bool allValid = PHX_TRUE;
        for (PHX_u32 index = 0; index < partitionEntryCount; index++)
        {
            const PHX_Byte* partitionEntry = partitionEntryArrayBuffer + index * partitionEntrySize;

            GUID guid = read_guid(partitionEntry + PHX_GPT_PENTRY_TYP);
            if (guid.first == 0 && guid.second == 0)
                continue;
            
            const PHX_u64 partitionFirstLba = read_u64(partitionEntry + PHX_GPT_PENTRY_FIL);
            const PHX_u64 partitionLastLba = read_u64(partitionEntry + PHX_GPT_PENTRY_LAL);

            partitionCount++;

            if (partitionFirstLba > partitionLastLba)
            {
                allValid = PHX_FALSE;
                break;
            }

            if (partitionFirstLba < firstUsableLba || partitionLastLba > lastUsableLba)
            {
                allValid = PHX_FALSE;
                break;
            }
        }

        if (allValid != PHX_TRUE)
        {
            context->allocator->free(context->allocator, partitionEntryArrayBuffer);
            continue;
        }

        foundHeader = PHX_TRUE;
        break;
    }

    if (foundHeader != PHX_TRUE)
    {
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    PHX_Partition* partitions = context->allocator->allocate(context->allocator, partitionCount * sizeof(PHX_Partition));
    if (!partitions)
    {
        context->allocator->free(context->allocator, partitionEntryArrayBuffer);
        context->allocator->free(context->allocator, bootsectorBuffer);
        return PHX_FALSE;
    }

    for (PHX_u32 index = 0; index < partitionEntryCount; index++)
    {
        PHX_Partition* partition = &partitions[index];
        const PHX_Byte* partitionEntry = partitionEntryArrayBuffer + index * partitionEntrySize;

        const GUID partitionTypeGuid = read_guid(partitionEntry + PHX_GPT_PENTRY_TYP);
        const GUID partitionGuid = read_guid(partitionEntry + PHX_GPT_PENTRY_PGU);
        const PHX_u64 partitionFirstLba = read_u64(partitionEntry + PHX_GPT_PENTRY_FIL);
        const PHX_u64 partitionLastLba = read_u64(partitionEntry + PHX_GPT_PENTRY_LAL);
        const PHX_u64 partitionAttributes = read_u64(partitionEntry + PHX_GPT_PENTRY_ATR);
        const PHX_u8* name16 = partitionEntry + PHX_GPT_PENTRY_NAM;

        partition->start = partitionFirstLba;
        partition->size = partitionLastLba - partitionFirstLba + 1;

        partition->signature.first = partitionGuid.first;
        partition->signature.second = partitionGuid.second;
        partition->signature.type = PHX_PARTITION_SIGNATURE_GUID;

        partition->flags = 0;
        if (partitionAttributes & PHX_GPT_ATTRIBUTE_REQUIRED_PARTITION)
            partition->flags |= PHX_PARTITION_REQUIRED;
        if (partitionAttributes & PHX_GPT_ATTRIBUTE_LEGACY_BOOTABLE)
            partition->flags |= PHX_PARTITION_BOOTABLE;

        if (is_guid(partitionTypeGuid, 0xC12A7328, 0xF81F, 0x11D2, 0xBA4B00A0C93EC93B))
            partition->type = PHX_PARTITION_ESP;
        else if (is_guid(partitionTypeGuid, 0xEBD0A0A2, 0xB9E5, 0x4433, 0x87C068B6B72699C7))
            partition->type = PHX_PARTITION_FAT32;
        else
            partition->type = PHX_PARTITION_UNKNOWN;

        // TODO
        (void)name16;
        memset(partition->name, '\0', sizeof(partition->name));
    }

    GUID diskGuid = read_guid(bootsectorBuffer + PHX_GPT_HEADER_DGUID);

    context->allocator->free(context->allocator, partitionEntryArrayBuffer);
    context->allocator->free(context->allocator, bootsectorBuffer);

    outTable->partitions = partitions;
    outTable->partitionCount = partitionCount;
    outTable->maxPartitionCount = partitionEntryCount;

    outTable->signature.first = diskGuid.first;
    outTable->signature.second = diskGuid.second;
    outTable->signature.type = PHX_PARTITION_SIGNATURE_GUID;
    
    outTable->startUsable = firstUsableLba;
    outTable->sizeUsable = lastUsableLba - firstUsableLba + 1;

    memcpy(outTable->bootsector, bootsector, 512);

    return PHX_TRUE;
}

static PHX_Bool GPT_WriteTable(PHX_Context* context, PHX_Disk_Device* device, const PHX_Partition_Table* table)
{
    if (table->partitionCount > table->maxPartitionCount)
        return PHX_FALSE;

    // TODO
    (void)write_guid;
    (void)write_u32;
    (void)write_u64;
    (void)MBR_EncodeCHS;
    (void)context;
    (void)device;
    return PHX_FALSE;
}

static PHX_Bool GPT_DefaultTable(PHX_Context* context, PHX_Disk_Device* device, PHX_Partition_Table* outTable)
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

*/

