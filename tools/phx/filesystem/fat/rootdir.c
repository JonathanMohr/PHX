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

PHX_Result PHX_Filesystem_FAT_FindFreeEntrySlots(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_u32 totalEntries, PHX_u32* clusterOut, PHX_u32* indexOut, PHX_u32* mainClusterOut, PHX_u32* mainIndexOut)
{
    PHX_Result result;
    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* extra = dir->extra;

    if (dir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32)
    {
        PHX_u32 runStart = 0;
        PHX_u32 runLength = 0;
        PHX_Bool runStarted = PHX_FALSE;

        for (PHX_u16 i = 0; i < data->specific.fat12_16.rootDirEntryCount; i++)
        {
            PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];
            if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, i, 1, entry)) != PHX_SUCCESS)
                return result;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            {
                if (runStarted != PHX_TRUE)
                {
                    runStart = i;
                    runLength = 0;
                }
                PHX_u32 remaining = (PHX_u32)data->specific.fat12_16.rootDirEntryCount - runStart;
                if (remaining >= totalEntries)
                {
                    *clusterOut = 0;
                    *indexOut = runStart;
                    *mainClusterOut = 0;
                    *mainIndexOut = runStart + totalEntries - 1;
                    return PHX_SUCCESS;
                }
                return PHX_ERROR_OUT_OF_SPACE;
            }

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED)
            {
                if (runStarted != PHX_TRUE)
                {
                    runStart = i;
                    runStarted = PHX_TRUE;
                    runLength = 0;
                }
                runLength++;
                if (runLength >= totalEntries)
                {
                    *clusterOut = 0;
                    *indexOut = runStart;
                    *mainClusterOut = 0;
                    *mainIndexOut = i;
                    return PHX_SUCCESS;
                }
            }
            else
            {
                runStarted = PHX_FALSE;
                runLength = 0;
            }
        }

        return PHX_ERROR_OUT_OF_SPACE;
    }

    const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;

    PHX_u32 runStartCluster = 0;
    PHX_u32 runStartIndex = 0;
    PHX_u32 runLength = 0;
    PHX_Bool runStarted = PHX_FALSE;

    PHX_u32 cluster = extra->startCluster;
    PHX_u32 lastCluster = cluster;

    while (1)
    {
        PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, cluster);
        if (status == PHX_FILESYSTEM_FAT_CLUSTER_ERROR)
            return PHX_ERROR_FORMAT;
        if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
            break;

        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;

        PHX_Byte* entries = data->clusterBuffer;

        for (PHX_u32 i = 0; i < entriesPerCluster; i++)
        {
            PHX_Byte* entry = entries + i * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            {
                if (runStarted != PHX_TRUE)
                {
                    runStartCluster = cluster;
                    runStartIndex = i;
                    runStarted = PHX_TRUE;
                }

                PHX_u32 offsetToMain = totalEntries - 1;
                PHX_u32 remainingInThisCluster = entriesPerCluster - i;

                PHX_Bool mainFound = (offsetToMain < remainingInThisCluster) ? PHX_TRUE : PHX_FALSE;
                PHX_u32 mainCluster = cluster;
                PHX_u32 mainIndex = (mainFound == PHX_TRUE) ? (i + offsetToMain) : 0;

                PHX_u32 remainingCapacity = remainingInThisCluster;
                PHX_u32 walkCluster = cluster;
                PHX_u32 chainLast = cluster;

                while (remainingCapacity < totalEntries)
                {
                    PHX_u32 nextCluster;
                    if ((result = PHX_Filesystem_FAT_ReadFAT(data, walkCluster, &nextCluster)) != PHX_SUCCESS)
                        return result;
                    PHX_u32 walkStatus = PHX_Filesystem_FAT_Cluster(data->version, nextCluster);
                    if (walkStatus == PHX_FILESYSTEM_FAT_CLUSTER_ERROR)
                        return false;
                    if (walkStatus != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                        break;

                    chainLast = nextCluster;
                    walkCluster = nextCluster;

                    if (mainFound != PHX_TRUE)
                    {
                        PHX_u32 offsetInThisCluster = offsetToMain - remainingCapacity;
                        if (offsetInThisCluster < entriesPerCluster)
                        {
                            mainCluster = nextCluster;
                            mainIndex = offsetInThisCluster;
                            mainFound = PHX_TRUE;
                        }
                    }

                    remainingCapacity += entriesPerCluster;
                }

                if (remainingCapacity >= totalEntries)
                {
                    *clusterOut = runStartCluster;
                    *indexOut = runStartIndex;
                    *mainClusterOut = mainCluster;
                    *mainIndexOut = mainIndex;
                    return PHX_SUCCESS;
                }

                PHX_u32 additionalNeeded = totalEntries - remainingCapacity;
                PHX_u32 additionalClusters = (additionalNeeded + entriesPerCluster - 1) / entriesPerCluster;

                PHX_u32 newFirst;
                if ((result = PHX_Filesystem_FAT_AppendClusters(data, chainLast, additionalClusters, &newFirst)) != PHX_SUCCESS)
                    return result;

                if (mainFound != PHX_TRUE)
                {
                    PHX_u32 offsetInNewChain = offsetToMain - remainingCapacity;
                    PHX_u32 newCluster = newFirst;
                    PHX_u32 remaining = offsetInNewChain;
                    while (remaining >= entriesPerCluster)
                    {
                        if ((result = PHX_Filesystem_FAT_ReadFAT(data, newCluster, &newCluster)) != PHX_SUCCESS)
                            return result;
                        remaining -= entriesPerCluster;
                    }
                    mainCluster = newCluster;
                    mainIndex = remaining;
                }

                *clusterOut = runStartCluster;
                *indexOut = runStartIndex;
                *mainClusterOut = mainCluster;
                *mainIndexOut = mainIndex;
            }
            else if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED)
            {
                if (runStarted != PHX_TRUE)
                {
                    runStartCluster = cluster;
                    runStartIndex = i;
                    runStarted = PHX_TRUE;
                    runLength = 0;
                }
                runLength++;
                if (runLength >= totalEntries)
                {
                    *clusterOut = runStartCluster;
                    *indexOut = runStartIndex;
                    *mainClusterOut = cluster;
                    *mainIndexOut = i;
                    return PHX_SUCCESS;
                }
            }
            else
            {
                runStarted = PHX_FALSE;
                runLength = 0;
            }
        }

        lastCluster = cluster;
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
            return result;
    }

    PHX_u32 stillNeeded = totalEntries - runLength;
    PHX_u32 additionalClusters = (stillNeeded + entriesPerCluster - 1) / entriesPerCluster;

    PHX_u32 newFirst;
    if ((result = PHX_Filesystem_FAT_AppendClusters(data, lastCluster, additionalClusters, &newFirst)) != PHX_SUCCESS)
        return result;

    PHX_u32 offsetToMain;

    if (runStarted != PHX_TRUE)
    {
        runStartCluster = newFirst;
        runStartIndex = 0;
        offsetToMain = totalEntries - 1;
    }
    else
        offsetToMain = (totalEntries - 1) - runLength;

    PHX_u32 mainCluster = newFirst;
    PHX_u32 remaining = offsetToMain;
    while (remaining >= entriesPerCluster)
    {
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, mainCluster, &mainCluster)) != PHX_SUCCESS)
            return result;
        remaining -= entriesPerCluster;
    }

    *clusterOut = runStartCluster;
    *indexOut = runStartIndex;
    *mainClusterOut = mainCluster;
    *mainIndexOut = remaining;
    return PHX_SUCCESS;
}
