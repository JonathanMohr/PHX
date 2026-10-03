#include "fat.h"
#include "result.h"

PHX_Result PHX_Filesystem_FAT_ReadRootDirectoryEntry(PHX_Filesystem_FAT_Data* data, PHX_u16 index, PHX_Byte* outEntry)
{
    if (data->version != PHX_FILESYSTEM_FAT_12 && data->version != PHX_FILESYSTEM_FAT_16)
        return PHX_ERROR_INTERNAL;

    if ((PHX_u32)index >= data->specific.fat12_16.rootDirEntryCount)
        return PHX_ERROR_INTERNAL;

    const PHX_u32 sector = data->specific.fat12_16.rootDirSector + (index / data->bytesPerSector);
    const PHX_u32 offsetInSector = index % data->bytesPerSector;

    if (data->usedDevice->read(data->usedDevice, data->buffer, sector, 1) != 1)
        return PHX_ERROR_IO;

    memcpy(outEntry, data->buffer + offsetInSector, PHX_FILESYSTEM_FAT_DIRENT_SIZE);

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_WriteRootDirectoryEntry(PHX_Filesystem_FAT_Data* data, PHX_u16 index, const PHX_Byte* entry)
{
    if (data->version != PHX_FILESYSTEM_FAT_12 && data->version != PHX_FILESYSTEM_FAT_16)
        return PHX_ERROR_INTERNAL;

    if ((PHX_u32)index >= data->specific.fat12_16.rootDirEntryCount)
        return PHX_ERROR_INTERNAL;

    const PHX_u32 sector = data->specific.fat12_16.rootDirSector + (index / data->bytesPerSector);
    const PHX_u32 offsetInSector = index % data->bytesPerSector;

    if (data->usedDevice->read(data->usedDevice, data->buffer, sector, 1) != 1)
        return PHX_ERROR_IO;

    memcpy(data->buffer + offsetInSector, entry, PHX_FILESYSTEM_FAT_DIRENT_SIZE);

    if (data->usedDevice->write(data->usedDevice, data->buffer, sector, 1) != 1)
        return PHX_ERROR_IO;

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_WriteEntries(PHX_Filesystem* fs, PHX_u32 entryCluster, PHX_u32 entryIndex, const void* entries, PHX_u32 totalEntries)
{
    // TODO
    return PHX_ERROR_INTERNAL;
}
