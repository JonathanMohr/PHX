#include "fat.h"
#include "device/device.h"
#include "filesystem/filesystem.h"
#include "types.h"

#include <endianness.h>
#include <base.h>

#include <embed/fat.h>

static struct PHX_Filesystem_Operations PHX_Filesystem_FAT_Operations = {
    PHX_Filesystem_FAT_ChangeBootsector,
    PHX_Filesystem_FAT_Destroy,
    
    PHX_NULL, // PHX_Filesystem_FAT_GetRoot,
    PHX_NULL, // PHX_Filesystem_FAT_GetNode,
    PHX_NULL, // PHX_Filesystem_FAT_RemoveNode,
    PHX_NULL, // PHX_Filesystem_FAT_CleanupNode,

    PHX_NULL, // PHX_Filesystem_FAT_Dir_GetEntryCount,
    PHX_NULL, // PHX_Filesystem_FAT_Dir_ReadEntry,
    PHX_NULL, // PHX_Filesystem_FAT_Dir_LookupEntry,

    PHX_NULL, // PHX_Filesystem_FAT_File_Read,
    PHX_NULL, // PHX_Filesystem_FAT_File_Write,
    PHX_NULL, // PHX_Filesystem_FAT_File_Seek,

    PHX_NULL, // PHX_Filesystem_FAT_CreateNode,

    PHX_NULL, // PHX_Filesystem_FAT_LinkEntry,
    PHX_NULL, // PHX_Filesystem_FAT_UnlinkEntry,
    PHX_NULL, // PHX_Filesystem_FAT_MoveEntry,

    PHX_Filesystem_FAT_CreateOpenNode,
    PHX_Filesystem_FAT_CloseOpenNode,
    PHX_Filesystem_FAT_ResetOpenNode
};


PHX_Bool PHX_Filesystem_FAT_WriteBootsector(PHX_Filesystem_FAT_Data* data)
{
    if (data->usedDevice->read(data->usedDevice, data->buffer, 0, 1) != 1)
        return PHX_FALSE;
    memcpy(data->buffer, data->bootsector, 512);
    if (data->usedDevice->write(data->usedDevice, data->buffer, 0, 1) != 1)
        return PHX_FALSE;

    if (data->version == PHX_FILESYSTEM_FAT_32 && data->specific.fat32.backupBootsector != 0)
    {
        if (data->usedDevice->read(data->usedDevice, data->buffer, data->specific.fat32.backupBootsector, 1) != 1)
            return PHX_FALSE;
        memcpy(data->buffer, data->bootsector, 512);
        if (data->usedDevice->write(data->usedDevice, data->buffer, data->specific.fat32.backupBootsector, 1) != 1)
            return PHX_FALSE;
    }

    return PHX_TRUE;
}

PHX_Bool PHX_Filesystem_FAT_ReadFsInfo(PHX_Filesystem_FAT_Data* data)
{
    if (data->version != PHX_FILESYSTEM_FAT_32 || data->specific.fat32.fsInfoSector == 0xFFFF)
        return PHX_FALSE;
    if (data->usedDevice->read(data->usedDevice, data->buffer, data->specific.fat32.fsInfoSector, 1) != 1)
        return PHX_FALSE;
    memcpy(data->fsInfo, data->buffer, 512);
    return PHX_TRUE;
}

PHX_Bool PHX_Filesystem_FAT_WriteFsInfo(PHX_Filesystem_FAT_Data* data)
{
    if (data->version != PHX_FILESYSTEM_FAT_32)
        return PHX_FALSE;
    if (data->usedDevice->read(data->usedDevice, data->buffer, data->specific.fat32.fsInfoSector, 1) != 1)
        return PHX_FALSE;
    memcpy(data->buffer, data->fsInfo, 512);
    if (data->usedDevice->write(data->usedDevice, data->buffer, data->specific.fat32.fsInfoSector, 1) != 1)
        return PHX_FALSE;
    return PHX_TRUE;
}



static PHX_Byte read_u8(const PHX_Byte* buffer)
{
    PHX_Byte val;
    memcpy(&val, buffer, sizeof(val));
    return val;
}

static PHX_u16 read_u16(const PHX_Byte* buffer)
{
    PHX_u16 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u16_Le(val);
}

static PHX_u32 read_u32(const PHX_Byte* buffer)
{
    PHX_u32 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u32_Le(val);
}

static void write_u8(PHX_Byte* buffer, PHX_Byte val)
{
    memcpy(buffer, &val, sizeof(val));
}

static void write_u16(PHX_Byte* buffer, PHX_u16 val)
{
    PHX_u16 rawVal = Endian_Convert_u16_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}

static void write_u32(PHX_Byte* buffer, PHX_u32 val)
{
    PHX_u32 rawVal = Endian_Convert_u32_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}


void PHX_Filesystem_FAT_UpdateFsInfo(PHX_Filesystem_FAT_Data* data)
{
    if (data->version != PHX_FILESYSTEM_FAT_32) return;

    memset(data->fsInfo, 0, 512);

    write_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_LES, PHX_FILESYSTEM_FAT_FSINFO_LEAD_SIGNATURE);

    write_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_STS, PHX_FILESYSTEM_FAT_FSINFO_STRUCT_SIGNATURE);

    write_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_FCC, data->freeClusterCount);
    write_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_NFC, data->nextFreeCluster);

    write_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_TRS, PHX_FILESYSTEM_FAT_FSINFO_TRAIL_SIGNATURE);
}


static void PHX_Filesystem_FAT_WriteBootsectorBuffer(
    PHX_Filesystem_FAT_Data* data, const char oemIdentifier[8],
    PHX_u16 sectorsPerTrack, PHX_u16 numberOfHeads,
    PHX_u32 hiddenSectors, PHX_Byte driveNumber,
    PHX_u32 volumeId,
    const char volumeLabel[11], const char filesystemType[8]
)
{
    PHX_Byte* bootsector = data->bootsector;

    memcpy(bootsector + PHX_FILESYSTEM_FAT_HEADER_OEM, oemIdentifier, 8);

    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_BPS, data->bytesPerSector);
    write_u8(bootsector + PHX_FILESYSTEM_FAT_HEADER_SPC, (PHX_Byte)data->sectorsPerCluster);
    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_RES, data->reservedSectors);

    write_u8(bootsector + PHX_FILESYSTEM_FAT_HEADER_FAC, data->fatCount);
    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_RDC, (data->version == PHX_FILESYSTEM_FAT_32) ? 0 : data->specific.fat12_16.rootDirEntryCount);
    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_TOS, (data->totalSectors <= 0xFFFF) ? (PHX_u16)data->totalSectors : 0);

    write_u8(bootsector + PHX_FILESYSTEM_FAT_HEADER_MED, data->mediaDescriptor);

    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_FAS, (data->version == PHX_FILESYSTEM_FAT_32) ? 0 : (PHX_u16)(data->fatSize / data->fatCount));
    
    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_SPT, sectorsPerTrack);
    write_u16(bootsector + PHX_FILESYSTEM_FAT_HEADER_NOH, numberOfHeads);

    write_u32(bootsector + PHX_FILESYSTEM_FAT_HEADER_HIS, hiddenSectors);
    write_u32(bootsector + PHX_FILESYSTEM_FAT_HEADER_LTS, (data->totalSectors > 0xFFFF) ? data->totalSectors : 0);

    if (data->version == PHX_FILESYSTEM_FAT_12 || data->version == PHX_FILESYSTEM_FAT_16)
    {
        write_u8(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_DRN, driveNumber);
        *(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_RES) = 0;
        write_u8(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_BOS, PHX_FILESYSTEM_FAT_BOOT_SIGNATURE_EXTENDED_BOOT_SIGNATURE);

        write_u32(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART, volumeId);

        memcpy(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART + 4, volumeLabel, 11);
        memcpy(bootsector + PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART + 15, filesystemType, 8);
    }
    else
    {
        PHX_u16 extFlags = 0;
        if (data->activeFat == PHX_FILESYSTEM_FAT_ACTIVE_ALL)
            extFlags = 0;
        else
            extFlags = (1 << 7) | (data->activeFat & (1 | 2 | 4 | 8));

        write_u32(bootsector + PHX_FILESYSTEM_FAT32_HEADER_FAS32, (data->fatSize / data->fatCount));
        
        write_u16(bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXF, extFlags);
        write_u16(bootsector + PHX_FILESYSTEM_FAT32_HEADER_FSV, 0);

        write_u32(bootsector + PHX_FILESYSTEM_FAT32_HEADER_ROC, data->specific.fat32.rootDirCluster);
        
        write_u16(bootsector + PHX_FILESYSTEM_FAT32_HEADER_FIS, data->specific.fat32.fsInfoSector);
        write_u16(bootsector + PHX_FILESYSTEM_FAT32_HEADER_BBS, data->specific.fat32.backupBootsector);

        memset(bootsector + PHX_FILESYSTEM_FAT32_HEADER_RES12, 0, 12);

        write_u8(bootsector + PHX_FILESYSTEM_FAT32_HEADER_DRN, driveNumber);
        *(bootsector + PHX_FILESYSTEM_FAT32_HEADER_RES) = 0;
        write_u8(bootsector + PHX_FILESYSTEM_FAT32_HEADER_BOS, PHX_FILESYSTEM_FAT_BOOT_SIGNATURE_EXTENDED_BOOT_SIGNATURE);

        write_u32(bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXTSTART, volumeId);

        memcpy(bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXTSTART + 4, volumeLabel, 11);
        memcpy(bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXTSTART + 15, filesystemType, 8);
    }
}

static PHX_Result PHX_Filesystem_FAT_UpdateBootsector(PHX_Filesystem_FAT_Data* data, const PHX_Byte* bootsector, PHX_u64 id)
{
    if (bootsector)
        memcpy(data->bootsector, bootsector, 512);
    else
        memcpy(data->bootsector, binary_file_data, 512);

    const char* filesystemType;
    if (data->version == PHX_FILESYSTEM_FAT_12)
        filesystemType = "FAT12   ";
    else if (data->version == PHX_FILESYSTEM_FAT_16)
        filesystemType = "FAT16   ";
    else // FAT32
        filesystemType = "FAT32   ";

    PHX_Filesystem_FAT_WriteBootsectorBuffer(
        data,
        "LFS     ",
        (data->version == PHX_FILESYSTEM_FAT_12) ? 18 : 32,
        (data->version == PHX_FILESYSTEM_FAT_32) ? 8 : 2,
        (PHX_u32)data->usedDevice->sectorOffset,
        (data->version == PHX_FILESYSTEM_FAT_12) ? 0x00 : 0x80,
        (id == PHX_FILESYSTEM_NO_ID) ? 0 : (PHX_u32)id,
        "NO NAME    ",
        filesystemType
    );

    if (PHX_Filesystem_FAT_WriteBootsector(data) != PHX_TRUE)
        return PHX_ERROR_IO;

    return PHX_SUCCESS;
}


PHX_Result PHX_Filesystem_FAT_ChangeBootsector(PHX_Filesystem* fs, const PHX_Byte* bootsector)
{
    return PHX_Filesystem_FAT_UpdateBootsector(fs->data, bootsector, fs->id);
}


void PHX_Filesystem_FAT_Destroy(PHX_Filesystem* fs)
{
    PHX_Filesystem_FAT_Data* data = fs->data;

    if (data->useDevice)
    {
        data->usedDevice->close(data->usedDevice);
        fs->context->allocator.free(&fs->context->allocator, data->usedDevice);
    }

    fs->context->allocator.free(&fs->context->allocator, data->buffer);

    fs->context->allocator.free(&fs->context->allocator, data);
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

    const PHX_Byte mediaDescriptor = read_u8(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_MED);
    
    // const PHX_u16 sectorsPerTrack = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_SPT);
    // const PHX_u16 numberOfHeads = read_u16(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_NOH);

    // const PHX_u32 hiddenSectors = read_u32(data->bootsector + PHX_FILESYSTEM_FAT_HEADER_HIS);


    PHX_u32 bootSignature;
    PHX_u32 volumeID;


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


        bootSignature = read_u8(data->bootsector + PHX_FILESYSTEM_FAT1X_HEADER_BOS);
        volumeID = read_u32(data->bootsector + PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART);
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


        bootSignature = read_u8(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_BOS);
        volumeID = read_u32(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXTSTART);
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


    data->buffer = context->allocator.allocate(&context->allocator, bytesPerSector);
    if (!data->buffer)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_MEMORY;
    }


    data->version = version;

    data->fatSector = reservedSectors;
    data->fatSize = fatSectors;

    data->dataSector = nonDataSectors;
    data->dataSize = dataSectors;

    data->bytesPerCluster = (PHX_u32)bytesPerSector * (PHX_u32)sectorsPerCluster;
    data->totalSectors = totalSectors;

    data->bytesPerCluster = bytesPerSector;
    data->sectorsPerCluster = sectorsPerCluster;
    data->reservedSectors = reservedSectors;

    data->fatCount = fatCount;
    data->mediaDescriptor = mediaDescriptor;

    if (version != PHX_FILESYSTEM_FAT_32)
    {
        data->specific.fat12_16.rootDirSector = rootDirEntryStart;
        data->specific.fat12_16.rootDirEntryCount = rootDirEntryCount;
        data->activeFat = PHX_FILESYSTEM_FAT_ACTIVE_ALL;

        data->freeClusterCount = 0xFFFFFFFF;
        data->nextFreeCluster = 2;
    }
    else
    {
        const PHX_u16 extFlags = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_EXF);
        // const PHX_u16 fsVersion = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_FSV);
        
        const PHX_u32 rootCluster = read_u32(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_ROC);
        const PHX_u16 fsInfoSector = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_FIS);
        const PHX_u16 backupBootsector = read_u16(data->bootsector + PHX_FILESYSTEM_FAT32_HEADER_BBS);

        data->specific.fat32.rootDirCluster = rootCluster;
        data->specific.fat32.fsInfoSector = (fsInfoSector < reservedSectors || fsInfoSector == 0) ? fsInfoSector : 0xFFFF;
        data->specific.fat32.backupBootsector = backupBootsector;

        if (extFlags & (1 << 7))
        {
            data->activeFat = extFlags & (1 | 2 | 4 | 8);
        }
        else
            data->activeFat = PHX_FILESYSTEM_FAT_ACTIVE_ALL;

        if (fsInfoSector < reservedSectors && PHX_Filesystem_FAT_ReadFsInfo(data) != PHX_TRUE)
        {
            if (data->useDevice)
            {
                data->usedDevice->close(data->usedDevice);
                context->allocator.free(&context->allocator, data->usedDevice);
            }
            context->allocator.free(&context->allocator, data->buffer);
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_IO;
        }

        const PHX_u32 leadSignature = read_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_LES);
        const PHX_u32 structSignature = read_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_STS);
        const PHX_u32 trailSignature = read_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_TRS);

        if (leadSignature == PHX_FILESYSTEM_FAT_FSINFO_LEAD_SIGNATURE && structSignature == PHX_FILESYSTEM_FAT_FSINFO_STRUCT_SIGNATURE && trailSignature == PHX_FILESYSTEM_FAT_FSINFO_TRAIL_SIGNATURE)
        {
            data->freeClusterCount = read_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_FCC);
            data->nextFreeCluster = read_u32(data->fsInfo + PHX_FILESYSTEM_FAT_FSINFO_NFC);
        }
        else
        {
            data->freeClusterCount = 0xFFFFFFFF;
            data->nextFreeCluster = 2;
        }
    }


    outFs->context = context;
    outFs->device = device;

    outFs->data = data;

    outFs->ops = &PHX_Filesystem_FAT_Operations;

    outFs->id = (bootSignature == PHX_FILESYSTEM_FAT_BOOT_SIGNATURE_EXTENDED_BOOT_SIGNATURE) ? volumeID : PHX_FILESYSTEM_NO_ID;

    outFs->caseSensitive = PHX_FALSE;

    return PHX_SUCCESS;
}


static PHX_u32 ceilToPowerOfTwo32(PHX_u32 value)
{
    PHX_u32 result = 1;
    while (result < value)
        result <<= 1;
    return result;
}

static PHX_Bool fitsFatType(PHX_u16 reservedSectors, PHX_u16 rootEntryCount, PHX_Byte fatCount, PHX_Byte fatEntryBitCount, PHX_u32 minClusterCount, PHX_u32 maxClusterCount, PHX_u64 totalSectorCount, PHX_u16 bytesPerSector, PHX_u16 maxSectorsPerCluster, PHX_u32 minSectorsPerClusterFloor, PHX_u16* outSectorsPerCluster, PHX_u32* outFatSectors)
{
    if (totalSectorCount > 0xFFFFFFFF)
        return PHX_FALSE;

    const PHX_u32 totalSectorCount32 = (PHX_u32)totalSectorCount;
    const PHX_u32 rootDirSectorCount = ((PHX_u32)rootEntryCount * 32 + (PHX_u32)bytesPerSector - 1) / (PHX_u32)bytesPerSector;
    const PHX_u32 fixedSectorCount = (PHX_u32)reservedSectors + rootDirSectorCount;

    if (totalSectorCount32 <= fixedSectorCount)
        return PHX_FALSE;

    const PHX_u32 dataSectorCountUpperBound = totalSectorCount32 - fixedSectorCount;

    const PHX_u32 minSectorsPerClusterUnrounded = dataSectorCountUpperBound / maxClusterCount + ((dataSectorCountUpperBound % maxClusterCount != 0) ? 1 : 0);
    if (minSectorsPerClusterUnrounded > (PHX_u32)maxSectorsPerCluster)
        return PHX_FALSE;

    const PHX_u32 minSectorsPerCluster = ceilToPowerOfTwo32(minSectorsPerClusterUnrounded);

    for (PHX_Byte attempt = 0; attempt < 2; attempt++)
    {
        PHX_u32 sectorsPerCluster = (attempt == 0) ? minSectorsPerCluster / 2 : minSectorsPerCluster;
        if (sectorsPerCluster < minSectorsPerClusterFloor)
            sectorsPerCluster = minSectorsPerClusterFloor;
        if (sectorsPerCluster == 0 || sectorsPerCluster > (PHX_u32)maxSectorsPerCluster)
            continue;

        const PHX_u64 fatEntryCount = (PHX_u64)dataSectorCountUpperBound / (PHX_u64)sectorsPerCluster + 2;
        const PHX_u32 bitsPerSector = (PHX_u32)bytesPerSector * 8;
        const PHX_u32 fatSectorCount = (PHX_u32)((fatEntryCount * (PHX_u64)fatEntryBitCount + (PHX_u64)bitsPerSector - 1) / (PHX_u64)bitsPerSector);
        const PHX_u32 totalFatSectorCount = (PHX_u32)fatCount * fatSectorCount;

        if (dataSectorCountUpperBound <= totalFatSectorCount)
            continue;

        const PHX_u32 clusterCount = (dataSectorCountUpperBound - totalFatSectorCount) / sectorsPerCluster;
        if (clusterCount < minClusterCount || clusterCount > maxClusterCount)
            continue;

        *outFatSectors = fatSectorCount;
        *outSectorsPerCluster = (PHX_u16)sectorsPerCluster;
        return PHX_TRUE;
    }

    return PHX_FALSE;
}

#define allow64kCluster PHX_FALSE

static PHX_Result PHX_Filesystem_FAT_FormatFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs, const PHX_Byte* bootsector)
{
    PHX_Result result;

    if (device->sectorOffset > 0xFFFFFFFF)
        return PHX_ERROR_PARAMETER;

    PHX_Filesystem_FAT_Data* data = context->allocator.allocate(&context->allocator, sizeof(PHX_Filesystem_FAT_Data));
    if (!data)
        return PHX_ERROR_MEMORY;

    PHX_u16 bytesPerSector;
    if (device->blockSize <= 512)
        bytesPerSector = 512;
    else if (device->blockSize <= 1024)
        bytesPerSector = 1024;
    else if (device->blockSize <= 2048)
        bytesPerSector = 2048;
    else
        bytesPerSector = 4096;

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


    PHX_Filesystem_FAT_Version setVersion = PHX_FILESYSTEM_FAT_32;
    const PHX_Bool customVersion = PHX_FALSE;


    const PHX_u32 maxClusterSizeBytes = allow64kCluster ? 65536 : 32768;
    const PHX_u16 maxSectorsPerCluster = (PHX_u16)(maxClusterSizeBytes / (PHX_u32)bytesPerSector);


    PHX_u16 rootDirEntryCount = 0;
    const PHX_Bool customRootDirEntryCount = PHX_FALSE;

    PHX_u32 fatSectorCount;

    // bytesPerSector
    PHX_u16 sectorsPerCluster;
    PHX_u16 reservedSectors = 0;
    const PHX_Bool customReservedSectors = PHX_FALSE;

    PHX_u16 activeFAT = PHX_FILESYSTEM_FAT_ACTIVE_ALL;

    PHX_Byte fatCount = 2;
    PHX_Byte mediaDescriptor = PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_DISK; // TODO


    // if FAT32
    PHX_u16 backupBootsector = 0;
    const PHX_Bool customBackupBootsector = PHX_FALSE;


    static const PHX_u16 fat12_rootDirEntryCount = 224;
    static const PHX_u16 fat12_reservedSectors = 1;

    static const PHX_u16 fat16_rootDirEntryCount = 512;
    static const PHX_u16 fat16_reservedSectors = 8;

    static const PHX_u16 fat32_rootDirEntryCount = 0;
    static const PHX_u16 fat32_reservedSectors = 32;
    static const PHX_u16 fat32_fsInfoSector = 1;
    static const PHX_u16 fat32_backupBootsector = 6;

    const PHX_u32 fat32Floor = (customVersion == PHX_TRUE) ? 1 : (4096 / bytesPerSector);

    PHX_Filesystem_FAT_Version version;
    if ((customVersion == PHX_FALSE || setVersion == PHX_FILESYSTEM_FAT_32) && fitsFatType((customReservedSectors == PHX_TRUE) ? reservedSectors : fat32_reservedSectors, (customRootDirEntryCount == PHX_TRUE) ? rootDirEntryCount : fat32_rootDirEntryCount, fatCount, 32, 65525, 0x0FFFFFF4, data->usedDevice->blockCount, bytesPerSector, maxSectorsPerCluster, fat32Floor, &sectorsPerCluster, &fatSectorCount) == PHX_TRUE)
    {
        version = PHX_FILESYSTEM_FAT_32;
        if (customRootDirEntryCount != PHX_TRUE)
            rootDirEntryCount = fat32_rootDirEntryCount;
        if (customReservedSectors != PHX_TRUE)
            reservedSectors = fat32_reservedSectors;
    }
    else if ((customVersion == PHX_FALSE || setVersion == PHX_FILESYSTEM_FAT_16) && fitsFatType((customReservedSectors == PHX_TRUE) ? reservedSectors : fat16_reservedSectors, (customRootDirEntryCount == PHX_TRUE) ? rootDirEntryCount : fat16_rootDirEntryCount, fatCount, 16, 4085, 65524, data->usedDevice->blockCount, bytesPerSector, maxSectorsPerCluster, 1, &sectorsPerCluster, &fatSectorCount) == PHX_TRUE)
    {
        version = PHX_FILESYSTEM_FAT_16;
        if (customRootDirEntryCount != PHX_TRUE)
            rootDirEntryCount = fat16_rootDirEntryCount;
        if (customReservedSectors != PHX_TRUE)
            reservedSectors = fat16_reservedSectors;
    }
    else if ((customVersion == PHX_FALSE || setVersion == PHX_FILESYSTEM_FAT_12) && fitsFatType((customReservedSectors == PHX_TRUE) ? reservedSectors : fat12_reservedSectors, (customRootDirEntryCount == PHX_TRUE) ? rootDirEntryCount : fat12_rootDirEntryCount, fatCount, 12, 1, 4084, data->usedDevice->blockCount, bytesPerSector, maxSectorsPerCluster, 1, &sectorsPerCluster, &fatSectorCount) == PHX_TRUE)
    {
        version = PHX_FILESYSTEM_FAT_12;
        if (customRootDirEntryCount != PHX_TRUE)
            rootDirEntryCount = fat12_rootDirEntryCount;
        if (customReservedSectors != PHX_TRUE)
            reservedSectors = fat12_reservedSectors;
    }
    else
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_SIZE;
    }


    const PHX_u32 totalSectors = (PHX_u32)data->usedDevice->blockCount;

    
    if ((PHX_u32)reservedSectors >= totalSectors)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_INTERNAL;
    }

    const PHX_u64 fatSectors64 = (PHX_u64)fatCount * (PHX_u64)fatSectorCount;
    if (fatSectors64 > (PHX_u64)totalSectors - (PHX_u64)reservedSectors)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_INTERNAL;
    }
    const PHX_u32 fatSectors = (PHX_u32)fatSectors64;


    const PHX_u64 rootDirEntryStart64 = (PHX_u64)reservedSectors + (PHX_u64)fatSectors;
    if (rootDirEntryStart64 > (PHX_u64)totalSectors)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_INTERNAL;
    }
    const PHX_u32 rootDirEntryStart = (PHX_u32)rootDirEntryStart64;

    const PHX_u16 rootDirSectors = (PHX_u16)((((PHX_u32)rootDirEntryCount * 32) + ((PHX_u32)bytesPerSector - 1)) / (PHX_u32)bytesPerSector);

    const PHX_u64 nonDataSectors64 = (PHX_u64)rootDirEntryStart + (PHX_u64)rootDirSectors;
    if (nonDataSectors64 >= (PHX_u64)totalSectors)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_INTERNAL;
    }
    const PHX_u32 nonDataSectors = (PHX_u32)nonDataSectors64;

    const PHX_u32 dataSectors = totalSectors - nonDataSectors;
    const PHX_u32 dataClusters = dataSectors / (PHX_u32)sectorsPerCluster;
    if (dataClusters == 0)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_INTERNAL;
    }

    if (version == PHX_FILESYSTEM_FAT_12 || version == PHX_FILESYSTEM_FAT_16)
    {
        if (rootDirEntryCount == 0)
        {
            if (data->useDevice)
            {
                data->usedDevice->close(data->usedDevice);
                context->allocator.free(&context->allocator, data->usedDevice);
            }
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_PARAMETER;
        }

        data->specific.fat12_16.rootDirSector = rootDirEntryStart;
        data->specific.fat12_16.rootDirEntryCount = rootDirEntryCount;
    }
    else
    {
        if (customBackupBootsector == PHX_TRUE && backupBootsector != 0 && backupBootsector + 3 > reservedSectors)
        {
            if (data->useDevice)
            {
                data->usedDevice->close(data->usedDevice);
                context->allocator.free(&context->allocator, data->usedDevice);
            }
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_PARAMETER;
        }

        data->specific.fat32.rootDirCluster = 2;
        data->specific.fat32.fsInfoSector = fat32_fsInfoSector;
        data->specific.fat32.backupBootsector = (customBackupBootsector == PHX_TRUE) ? backupBootsector : fat32_backupBootsector;

        // TODO: Set root dir cluster
    }

    data->buffer = context->allocator.allocate(&context->allocator, bytesPerSector);
    if (!data->buffer)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_MEMORY;
    }

    data->version = version;

    data->freeClusterCount = dataSectors / (PHX_u32)sectorsPerCluster;
    data->nextFreeCluster = 3;

    data->fatSector = reservedSectors;
    data->fatSize = fatSectors;

    data->dataSector = nonDataSectors;
    data->dataSize = dataSectors;

    data->bytesPerCluster = (PHX_u32)bytesPerSector * (PHX_u32)sectorsPerCluster;
    data->totalSectors = totalSectors;


    data->bytesPerSector = bytesPerSector;
    data->sectorsPerCluster = sectorsPerCluster;
    data->reservedSectors = reservedSectors;

    data->activeFat = activeFAT;

    data->fatCount = fatCount;
    data->mediaDescriptor = mediaDescriptor;


    const PHX_u32 volumeId = PHX_Context_GetRandomU32(context);

    if ((result = PHX_Filesystem_FAT_UpdateBootsector(data, bootsector, volumeId)) != PHX_SUCCESS)
    {
        if (data->useDevice)
        {
            data->usedDevice->close(data->usedDevice);
            context->allocator.free(&context->allocator, data->usedDevice);
        }
        context->allocator.free(&context->allocator, data->buffer);
        context->allocator.free(&context->allocator, data);
        return result;
    }

    if (version == PHX_FILESYSTEM_FAT_32)
    {
        PHX_Filesystem_FAT_UpdateFsInfo(data);

        if (PHX_Filesystem_FAT_WriteFsInfo(data) != PHX_TRUE)
        {
            if (data->useDevice)
            {
                data->usedDevice->close(data->usedDevice);
                context->allocator.free(&context->allocator, data->usedDevice);
            }
            context->allocator.free(&context->allocator, data->buffer);
            context->allocator.free(&context->allocator, data);
            return PHX_ERROR_PARAMETER;
        }
    }

    outFs->context = context;
    outFs->device = device;

    outFs->data = data;

    outFs->ops = &PHX_Filesystem_FAT_Operations;

    outFs->id = volumeId;

    outFs->caseSensitive = PHX_FALSE;

    return PHX_SUCCESS;
}


PHX_Filesystem_Interface PHX_Filesystem_FAT_Interface = {
    PHX_Filesystem_FAT_OpenFilesystem,
    PHX_Filesystem_FAT_FormatFilesystem,
    PHX_FILESYSTEM_FAT_TYPE,
    "FAT-FILESYSTEM-INTERFACE"
};
