#include "fat.h"
#include "filesystem/filesystem.h"
#include "result.h"

PHX_Filesystem_Size PHX_Filesystem_FAT_File_Read(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size size, void* buffer)
{
    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_OpenNode_Extra* extra = openFile->extra;

    if (openFile->pos >= file->size)
        return 0;

    PHX_Filesystem_Size remainingInFile = file->size - openFile->pos;
    if (size > remainingInFile)
        size = remainingInFile;

    PHX_Filesystem_Size read = 0;
    PHX_u32 cluster = extra->currentCluster;
    PHX_u32 offsetInCluster = (PHX_u32)(openFile->pos % data->bytesPerCluster);

    if (offsetInCluster == 0 && openFile->pos != 0)
        offsetInCluster = data->bytesPerCluster;

    while (read < size)
    {
        if (offsetInCluster >= data->bytesPerCluster)
        {
            PHX_u32 next;
            if (PHX_Filesystem_FAT_ReadFAT(data, cluster, &next) != PHX_SUCCESS)
                break;
            if (PHX_Filesystem_FAT_Cluster(data->version, next) != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                break;

            cluster = next;
            offsetInCluster = 0;
        }

        PHX_u32 chunk = data->bytesPerCluster - offsetInCluster;
        PHX_Filesystem_Size remaining = size - read;
        if ((PHX_Filesystem_Size)chunk > remaining)
            chunk = (PHX_u32)remaining;

        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            break;

        memcpy((PHX_Byte*)buffer + read, (PHX_Byte*)data->clusterBuffer + offsetInCluster, chunk);
        
        read += chunk;
        openFile->pos += chunk;
        offsetInCluster += chunk;
        extra->currentCluster = cluster;
    }

    return read;
}

PHX_Filesystem_Size PHX_Filesystem_FAT_File_Write(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size size, const void* buffer)
{
    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* nodeExtra = file->extra;
    PHX_Filesystem_FAT_OpenNode_Extra* extra = openFile->extra;

    if (size == 0 || openFile->pos >= 0xFFFFFFFF)
        return 0;

    if (size > 0xFFFFFFFF - openFile->pos)
        size = 0xFFFFFFFF - openFile->pos;

    const PHX_Filesystem_Size oldSize = file->size;
    const PHX_u32 oldStart = nodeExtra->startCluster;
    const PHX_Filesystem_Size oldPos = openFile->pos;
    const PHX_u32 oldCurrent = extra->currentCluster;

    const PHX_Filesystem_Size gap = (openFile->pos > file->size) ? (openFile->pos - file->size) : 0;

    PHX_Bool startChanged = PHX_FALSE;
    PHX_u32 firstAllocated = 0;
    PHX_u32 cluster;

    if (nodeExtra->startCluster == 0)
    {
        PHX_u32 first;
        if (PHX_Filesystem_FAT_AppendClusters(data, 0, 1, &first) != PHX_SUCCESS)
            return 0;

        nodeExtra->startCluster = first;
        extra->currentCluster = first;
        cluster = first;
        firstAllocated = first;
        startChanged = PHX_TRUE;
    }
    else if (gap > 0)
    {
        cluster = nodeExtra->startCluster;

        if (file->size > 0)
        {
            const PHX_u32 targetIndex = (PHX_u32)((file->size - 1) / data->bytesPerCluster);
            for (PHX_u32 i = 0; i < targetIndex; i++)
            {
                PHX_u32 next;
                if (PHX_Filesystem_FAT_ReadFAT(data, cluster, &next) != PHX_SUCCESS)
                    return 0;
                if (PHX_Filesystem_FAT_Cluster(data->version, next) != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                    return 0;
                cluster = next;
            }
        }
    }
    else
        cluster = extra->currentCluster;

    if (gap > 0)
    {
        openFile->pos = file->size;
        extra->currentCluster = cluster;
    }

    PHX_u32 offsetInCluster = (PHX_u32)(openFile->pos % data->bytesPerCluster);
    if (offsetInCluster == 0 && openFile->pos != 0)
        offsetInCluster = data->bytesPerCluster;

    const PHX_Filesystem_Size total = gap + size;
    PHX_Filesystem_Size done = 0;

    while (done < total)
    {
        if (offsetInCluster >= data->bytesPerCluster)
        {
            PHX_u32 next;
            if (PHX_Filesystem_FAT_ReadFAT(data, cluster, &next) != PHX_SUCCESS)
                break;
            PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, next);
            if (status == PHX_FILESYSTEM_FAT_CLUSTER_EOC)
            {
                if (PHX_Filesystem_FAT_AppendClusters(data, cluster, 1, &next) != PHX_SUCCESS)
                    break;
            }
            else if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                break;

            cluster = next;
            offsetInCluster = 0;
        }

        PHX_Filesystem_Size limit = (done < gap) ? (gap - done) : (total - done);
        PHX_u32 chunk = data->bytesPerCluster - offsetInCluster;
        if ((PHX_Filesystem_Size)chunk > limit)
            chunk = (PHX_u32)limit;

        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            break;

        if (done < gap)
            memset((PHX_Byte*)data->clusterBuffer + offsetInCluster, 0, chunk);
        else
            memcpy((PHX_Byte*)data->clusterBuffer + offsetInCluster, (const PHX_Byte*)buffer + (done - gap), chunk);

        if (data->usedDevice->write(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            break;

        done += chunk;
        openFile->pos += chunk;
        offsetInCluster += chunk;
        extra->currentCluster = cluster;
    }

    PHX_Filesystem_Size written = (done > gap) ? (done - gap) : 0;

    PHX_Bool sizeChanged = PHX_FALSE;
    if (openFile->pos > file->size)
    {
        file->size = openFile->pos;
        sizeChanged = PHX_TRUE;
    }

    if (sizeChanged == PHX_TRUE || startChanged == PHX_TRUE)
    {
        PHX_u32 entryCluster = (PHX_u32)(file->number >> 32);
        PHX_u32 entryIndex = (PHX_u32)(file->number & 0xFFFFFFFF);

        PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];
        PHX_Result result;

        if (entryCluster == 0)
            result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, (PHX_u16)entryIndex, 1, entry);
        else
        {
            PHX_u32 indexInCluster = entryIndex % (data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE);
            PHX_u32 entryOffsetInCluster = indexInCluster * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            const PHX_u32 sectorOffset = entryOffsetInCluster / data->bytesPerSector;
            const PHX_u32 offsetInSector = entryOffsetInCluster % data->bytesPerSector;

            if (data->usedDevice->read(data->usedDevice, data->buffer, PHX_Filesystem_FAT_GetClusterStart(data, entryCluster) + sectorOffset, 1) != 1)
                result = PHX_ERROR_IO;
            else
            {
                memcpy(entry, data->buffer + offsetInSector, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
                result = PHX_SUCCESS;
            }
        }

        if (result == PHX_SUCCESS)
        {
            write_u32(entry + PHX_FILESYSTEM_FAT_DIRENT_FIS, (PHX_u32)file->size);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCL, (PHX_u16)(nodeExtra->startCluster & 0xFFFF));
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCH, (data->version == PHX_FILESYSTEM_FAT_32) ? (PHX_u16)(nodeExtra->startCluster >> 16) : 0);
            result = PHX_Filesystem_FAT_WriteEntries(fs, entryCluster, entryIndex, entry, 1);
        }

        if (result != PHX_SUCCESS)
        {
            file->size = oldSize;
            nodeExtra->startCluster = oldStart;
            openFile->pos = oldPos;
            extra->currentCluster = oldCurrent;

            if (startChanged == PHX_TRUE)
            {
                PHX_u32 c = firstAllocated;
                while (PHX_Filesystem_FAT_Cluster(data->version, c) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                {
                    PHX_u32 next;
                    if (PHX_Filesystem_FAT_ReadFAT(data, c, &next) != PHX_SUCCESS)
                        break;
                    if (PHX_Filesystem_FAT_WriteFAT(data, c, 0) != PHX_SUCCESS)
                        break;
                    c = next;
                }
            }

            return 0;
        }
    }

    return written;
}

PHX_Result PHX_Filesystem_FAT_File_Seek(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size pos)
{
    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* nodeExtra = file->extra;
    PHX_Filesystem_FAT_OpenNode_Extra* extra = openFile->extra;

    if (pos > 0xFFFFFFFF)
        return PHX_ERROR_NOT_SUPPORTED;

    if (pos == 0)
    {
        openFile->pos = 0;
        extra->currentCluster = nodeExtra->startCluster;
        return PHX_SUCCESS;
    }

    if (pos > file->size)
    {
        openFile->pos = pos;
        return PHX_SUCCESS;
    }

    const PHX_u32 targetIndex = (PHX_u32)((pos - 1) / data->bytesPerCluster);

    PHX_u32 cluster = nodeExtra->startCluster;
    PHX_u32 index = 0;

    if (openFile->pos > 0 && openFile->pos <= file->size)
    {
        PHX_u32 currentIndex = (PHX_u32)((openFile->pos - 1) / data->bytesPerCluster);
        if (currentIndex <= targetIndex)
        {
            cluster = extra->currentCluster;
            index = currentIndex;
        }
    }

    while (index < targetIndex)
    {
        PHX_Result result;
        PHX_u32 next;
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &next)) != PHX_SUCCESS)
            return result;
        if (PHX_Filesystem_FAT_Cluster(data->version, next) != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
            return PHX_ERROR_FORMAT;

        cluster = next;
        index++;
    }

    openFile->pos = pos;
    extra->currentCluster = cluster;
    return PHX_SUCCESS;
}
