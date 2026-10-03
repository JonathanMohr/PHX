#include "fat.h"
#include "result.h"

PHX_Result PHX_Filesystem_FAT_ReadRootDirectoryEntries(PHX_Filesystem_FAT_Data* data, PHX_u16 index, PHX_u16 count, PHX_Byte* outEntries)
{
    if (data->version != PHX_FILESYSTEM_FAT_12 && data->version != PHX_FILESYSTEM_FAT_16)
        return PHX_ERROR_INTERNAL;

    if (count == 0)
        return PHX_SUCCESS;

    if ((PHX_u32)index + (PHX_u32)count > data->specific.fat12_16.rootDirEntryCount)
        return PHX_ERROR_INTERNAL;

    const PHX_u32 entriesPerSector = data->bytesPerSector / PHX_FILESYSTEM_FAT_DIRENT_SIZE;

    PHX_u32 sector = data->specific.fat12_16.rootDirSector + (index / entriesPerSector);
    PHX_u32 entryInSector = index % entriesPerSector;
    PHX_u32 remaining = count;
    PHX_Byte* out = outEntries;

    while (remaining > 0)
    {
        PHX_u32 n = entriesPerSector - entryInSector;
        if (n > remaining)
            n = remaining;

        if (data->usedDevice->read(data->usedDevice, data->buffer, sector, 1) != 1)
            return PHX_ERROR_IO;

        memcpy(out, data->buffer + entryInSector * PHX_FILESYSTEM_FAT_DIRENT_SIZE, n * PHX_FILESYSTEM_FAT_DIRENT_SIZE);

        out += n * PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        remaining -= n;
        entryInSector = 0;
        sector++;
    }

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_WriteRootDirectoryEntries(PHX_Filesystem_FAT_Data* data, PHX_u16 index, PHX_u16 count, const PHX_Byte* entries)
{
    if (data->version != PHX_FILESYSTEM_FAT_12 && data->version != PHX_FILESYSTEM_FAT_16)
        return PHX_ERROR_INTERNAL;

    if (count == 0)
        return PHX_SUCCESS;

    if ((PHX_u32)index + (PHX_u32)count > data->specific.fat12_16.rootDirEntryCount)
        return PHX_ERROR_INTERNAL;

    const PHX_u32 entriesPerSector = data->bytesPerSector / PHX_FILESYSTEM_FAT_DIRENT_SIZE;

    PHX_u32 sector = data->specific.fat12_16.rootDirSector + (index / entriesPerSector);
    PHX_u32 entryInSector = index % entriesPerSector;
    PHX_u32 remaining = count;
    const PHX_Byte* in = entries;

    while (remaining > 0)
    {
        PHX_u32 n = entriesPerSector - entryInSector;
        if (n > remaining)
            n = remaining;

        if ((entryInSector != 0 || n != entriesPerSector) && data->usedDevice->read(data->usedDevice, data->buffer, sector, 1) != 1)
            return PHX_ERROR_IO;

        memcpy(data->buffer + entryInSector * PHX_FILESYSTEM_FAT_DIRENT_SIZE, in, n * PHX_FILESYSTEM_FAT_DIRENT_SIZE);

        if (data->usedDevice->write(data->usedDevice, data->buffer, sector, 1) != 1)
            return PHX_ERROR_IO;

        in += n * PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        remaining -= n;
        entryInSector = 0;
        sector++;
    }

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_WriteEntries(PHX_Filesystem* fs, PHX_u32 entryCluster, PHX_u32 entryIndex, const void* entries, PHX_u32 totalEntries)
{
    PHX_Result result;
    PHX_Filesystem_FAT_Data* data = fs->data;

    if (entryCluster == 0)
    {
        if (entryIndex > 0xFFFF || totalEntries > 0xFFFF)
            return PHX_ERROR_INTERNAL;
        return PHX_Filesystem_FAT_WriteRootDirectoryEntries(data, (PHX_u16)entryIndex, (PHX_u16)totalEntries, entries);
    }

    const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;
    const PHX_Byte* src = entries;

    PHX_u32 remaining = totalEntries;
    PHX_u32 cluster = entryCluster;
    PHX_u32 indexInCluster = entryIndex;

    while (remaining > 0)
    {
        PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, cluster);
        if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
            return PHX_ERROR_FORMAT;

        const PHX_u32 availableInCluster = entriesPerCluster - indexInCluster;
        const PHX_u32 chunk = (remaining < availableInCluster) ? remaining : availableInCluster;

        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;

        memcpy(data->clusterBuffer + (PHX_u64)indexInCluster * PHX_FILESYSTEM_FAT_DIRENT_SIZE, src, (PHX_u64)chunk * PHX_FILESYSTEM_FAT_DIRENT_SIZE);

        if (data->usedDevice->write(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;

        src += (PHX_u64)chunk * PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        remaining -= chunk;

        if (remaining > 0)
        {
            if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
                return result;
            indexInCluster = 0;
        }
    }

    return PHX_SUCCESS;
}
