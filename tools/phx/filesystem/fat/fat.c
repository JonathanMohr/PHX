#include "fat.h"
#include "device/device.h"

#include <endianness.h>
#include <base.h>

#include <embed/fat.h>

/*
static struct PHX_Filesystem_Operations PHX_Filesystem_FAT_Operations = {
    PHX_Filesystem_FAT_ChangeBootsector,
    PHX_Filesystem_FAT_Destroy,
    
    PHX_Filesystem_FAT_GetRoot,
    PHX_Filesystem_FAT_GetNode,
    PHX_Filesystem_FAT_RemoveNode,
    PHX_Filesystem_FAT_CleanupNode,

    PHX_Filesystem_FAT_Dir_GetEntryCount,
    PHX_Filesystem_FAT_Dir_ReadEntry,
    PHX_Filesystem_FAT_Dir_LookupEntry,

    PHX_Filesystem_FAT_File_Read,
    PHX_Filesystem_FAT_File_Write,
    PHX_Filesystem_FAT_File_Seek,

    PHX_Filesystem_FAT_CreateNode,

    PHX_Filesystem_FAT_LinkEntry,
    PHX_Filesystem_FAT_UnlinkEntry,
    PHX_Filesystem_FAT_MoveEntry,

    PHX_Filesystem_FAT_CreateOpenNode,
    PHX_Filesystem_FAT_CloseOpenNode,
    PHX_Filesystem_FAT_ResetOpenNode
};
*/

static PHX_Byte read_u8(PHX_Byte* buffer)
{
    PHX_Byte val;
    memcpy(&val, buffer, sizeof(val));
    return val;
}

static PHX_u16 read_u16(PHX_Byte* buffer)
{
    PHX_u16 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u16_Le(val);
}

static PHX_u32 read_u32(PHX_Byte* buffer)
{
    PHX_u32 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u32_Le(val);
}


// TODO: Check
static int isPowerOfTwo(PHX_u16 v) { return v && !(v & (v - 1)); }

static PHX_Result PHX_Filesystem_FAT_OpenFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs)
{
    PHX_Filesystem_FAT_Data* data = context->allocator.allocate(&context->allocator, sizeof(PHX_Filesystem_FAT_Data));
    if (!data)
        return PHX_ERROR_MEMORY;

    const PHX_BlockSize blocksForBootsector = (512 + device->blockSize - 1) / device->blockSize;

    PHX_Byte* sectorBuffer = context->allocator.allocate(&context->allocator, blocksForBootsector);
    if (!sectorBuffer)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_MEMORY;
    }

    if (device->read(device, sectorBuffer, 0, blocksForBootsector) != blocksForBootsector)
    {
        context->allocator.free(&context->allocator, data);
        context->allocator.free(&context->allocator, sectorBuffer);
        return PHX_ERROR_IO;
    }

    memcpy(data->bootsector, sectorBuffer, 512);

    context->allocator.free(&context->allocator, sectorBuffer);

    const PHX_u16 bytesPerSector = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_BPS);
    const PHX_Byte sectorsPerCluster = read_u8(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_SPC);
    const PHX_u16 reservedSectors = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_RES);

    const PHX_Byte fatCount = read_u8(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_FAC);
    const PHX_u16 rootDirEntryCount = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_RDC);
    const PHX_u32 totalSectors = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_TOS) ? read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_TOS) : read_u32(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_LTS);

    const PHX_u16 fatSize16 = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_FAS);
    const PHX_u32 fatSize = fatSize16 ? fatSize16 : read_u32(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_FAS32);

    // const PHX_Byte mediaDescriptor = read_u8(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_MED);
    
    // const PHX_u16 sectorsPerTrack = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_SPT);
    // const PHX_u16 numberOfHeads = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_NOH);

    // const PHX_u32 hiddenSectors = read_u32(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_HIS);


    if (data->bootsector[510] != 0x55 ||data->bootsector[511] != 0xAA)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }

    if (bytesPerSector != 512 && bytesPerSector != 1024 &&
        bytesPerSector != 2048 && bytesPerSector != 4096)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }

    if (!isPowerOfTwo(sectorsPerCluster) || sectorsPerCluster > 128)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }

    if (fatSize == 0 || totalSectors == 0 || fatCount == 0 || reservedSectors == 0 || reservedSectors >= totalSectors)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }


    const PHX_u64 fatSectors64 = (PHX_u64)fatCount * (PHX_u64)fatSize;
    if (fatSectors64 >= (PHX_u64)totalSectors - (PHX_u64)reservedSectors)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }
    const PHX_u32 fatSectors = (PHX_u32)fatSectors64;

    const PHX_u32 rootDirEntryStart = (PHX_u32)reservedSectors + fatSectors;
    const PHX_u32 rootDirSectors = (((PHX_u32)rootDirEntryCount * 32) + (PHX_u32)(bytesPerSector - 1)) / (PHX_u32)bytesPerSector;

    const PHX_u64 nonDataSectors64 = (PHX_u64)rootDirEntryStart + (PHX_u64)rootDirSectors;
    if (nonDataSectors64 >= (PHX_u64)totalSectors)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }
    const PHX_u32 nonDataSectors = (PHX_u32)nonDataSectors64;

    const PHX_u32 dataSectors = totalSectors - nonDataSectors;
    const PHX_u32 dataClusters = dataSectors / (PHX_u32)sectorsPerCluster;
    if (dataClusters == 0)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_FORMAT;
    }


    PHX_Filesystem_FAT_Version version;
    if (fatSize16 != 0)
    {
        if (rootDirEntryCount == 0)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_FORMAT;
        }

        if (dataClusters <= 4084)
            version = PHX_FILESYSTEM_FAT_12;
        else
            version = PHX_FILESYSTEM_FAT_16;
    }
    else
    {
        // const PHX_u16 extFlags = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXF);
        const PHX_u16 fsVersion = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_FSV);
        
        // const PHX_u32 rootCluster = read_u32(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_ROC);
        // const PHX_u16 fsInfoSector = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_FIS);
        const PHX_u16 backupBootsector = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_BBS);

        if (fsVersion != 0)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_FORMAT;
        }

        if (backupBootsector != 0 && backupBootsector + 3 > reservedSectors)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_FORMAT;
        }

        if (rootDirEntryCount != 0)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_FORMAT;
        }

        version = PHX_FILESYSTEM_FAT_32;
    }

    
    if (device->blockSize != bytesPerSector)
    {
        PHX_BlockDevice* usedDevice = context->allocator.allocate(&context->allocator, sizeof(PHX_BlockDevice));
        if (!usedDevice)
        {
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_INTERNAL;
        }
        if (PHX_BlockCountTransformDevice(context, device, bytesPerSector, PHX_FALSE, usedDevice) != PHX_TRUE)
        {
            context->allocator.free(&context->allocator, usedDevice);
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_INTERNAL;
        }

        data->usedDevice = usedDevice;
        data->useDevice = PHX_TRUE;
    }
    else
    {
        data->usedDevice = device;
        data->useDevice = PHX_FALSE;
    }


    


    (void)outFs;

    return PHX_ERROR_INTERNAL;
}


static PHX_Result PHX_Filesystem_FAT_FormatFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs, const PHX_Byte* bootsector)
{


    (void)context;
    (void)device;
    (void)outFs;
    (void)bootsector;
    return PHX_ERROR_INTERNAL;
}


PHX_Filesystem_Interface PHX_Filesystem_FAT_Interface = {
    PHX_Filesystem_FAT_OpenFilesystem,
    PHX_Filesystem_FAT_FormatFilesystem,
    PHX_FILESYSTEM_FAT_TYPE,
    "FAT-FILESYSTEM-INTERFACE"
};
