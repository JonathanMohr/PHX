#include "fat.h"

PHX_Result PHX_Filesystem_FAT_GetNode(PHX_Filesystem* fs, PHX_Filesystem_NodeNumber number, PHX_Filesystem_Node* nodeOut)
{
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* extra = fs->context->allocator.allocate(&fs->context->allocator, sizeof(PHX_Filesystem_FAT_Node_Extra));
    if (!extra)
        return PHX_ERROR_MEMORY;

    if (number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT)
    {
        if (data->version == PHX_FILESYSTEM_FAT_32)
        {
            extra->startCluster = data->specific.fat32.rootDirCluster;

            PHX_u64 size = 0;
            PHX_u32 status;
            PHX_u32 chainLength = 0;

            PHX_u32 cluster = data->specific.fat32.rootDirCluster;
            while ((status = PHX_Filesystem_FAT_Cluster(data->version, cluster)) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
            {
                if (++chainLength > data->totalClusters)
                {
                    fs->context->allocator.free(&fs->context->allocator, extra);
                    return PHX_ERROR_FORMAT;
                }

                size += (PHX_u64)data->bytesPerSector * (PHX_u64)data->sectorsPerCluster;
                if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
                {
                    fs->context->allocator.free(&fs->context->allocator, extra);
                    return result;
                }
            }

            if (status != PHX_FILESYSTEM_FAT_CLUSTER_EOC)
            {
                fs->context->allocator.free(&fs->context->allocator, extra);
                return PHX_ERROR_FORMAT;
            }

            nodeOut->size = size;
        }
        else
        {
            extra->startCluster = 0;
            nodeOut->size = data->specific.fat12_16.rootDirEntryCount * PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        }

        nodeOut->number = PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT;
        nodeOut->referenceCount = 1;

        nodeOut->extra = extra;

        nodeOut->attributes = 0;
        nodeOut->type = PHX_FILESYSTEM_ENTRY_DIRECTORY;

        return PHX_SUCCESS;
    }

    PHX_u32 cluster = (PHX_u32)(number >> 32);
    PHX_u32 index32 = (PHX_u32)(number & 0xFFFFFFFF);
    if (index32 > 0xFFFF)
    {
        fs->context->allocator.free(&fs->context->allocator, extra);
        return PHX_ERROR_INTERNAL;
    }

    PHX_u16 index = (PHX_u16)index32;

    PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];

    if (cluster == 0)
    {
        if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntry(data, index, entry)) != PHX_SUCCESS)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return result;
        }
    }
    else
    {
        if (cluster < 2 || cluster >= data->totalClusters + 2)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return PHX_ERROR_INTERNAL;
        }

        const PHX_u32 offsetInCluster = (PHX_u32)index * PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        if (offsetInCluster >= data->bytesPerCluster)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return PHX_ERROR_INTERNAL;
        }

        const PHX_u32 sectorOffset = offsetInCluster / data->bytesPerSector;
        const PHX_u32 offsetInSector = offsetInCluster % data->bytesPerSector;

        if (data->usedDevice->read(data->usedDevice, data->buffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster) + sectorOffset, 1) != 1)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return PHX_ERROR_IO;
        }

        memcpy(entry, data->buffer + offsetInSector, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
    }

    const PHX_Byte attributes = read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR);

    extra->startCluster = read_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCL);
    if (data->version == PHX_FILESYSTEM_FAT_32)
        extra->startCluster |= (PHX_u32)read_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCH) << 16;

    PHX_u64 size = 0;
    if (attributes & PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY)
    {
        if (extra->startCluster == 0)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return PHX_Filesystem_FAT_GetNode(fs, PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT, nodeOut);
        }

        PHX_u32 status;
        PHX_u32 chainLength = 0;

        PHX_u32 dirCluster = extra->startCluster;
        while ((status = PHX_Filesystem_FAT_Cluster(data->version, dirCluster)) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
        {
            if (++chainLength > data->totalClusters)
            {
                fs->context->allocator.free(&fs->context->allocator, extra);
                return PHX_ERROR_FORMAT;
            }
            
            size += (PHX_u64)data->bytesPerSector * (PHX_u64)data->sectorsPerCluster;
            if ((result = PHX_Filesystem_FAT_ReadFAT(data, dirCluster, &dirCluster)) != PHX_SUCCESS)
            {
                fs->context->allocator.free(&fs->context->allocator, extra);
                return result;
            }
        }

        if (status != PHX_FILESYSTEM_FAT_CLUSTER_EOC)
        {
            fs->context->allocator.free(&fs->context->allocator, extra);
            return PHX_ERROR_FORMAT;
        }
    }
    else
        size = (PHX_u64)read_u32(entry + PHX_FILESYSTEM_FAT_DIRENT_FIS);

    nodeOut->number = number;
    nodeOut->size = size;
    nodeOut->referenceCount = 1;

    nodeOut->extra = extra;

    nodeOut->attributes = 0;
    if (attributes & PHX_FILESYSTEM_FAT_ENTRY_READONLY)
        nodeOut->attributes |= PHX_FILESYSTEM_ATTRIBUTE_READONLY;
    if (attributes & PHX_FILESYSTEM_FAT_ENTRY_HIDDEN)
        nodeOut->attributes |= PHX_FILESYSTEM_ATTRIBUTE_HIDDEN;
    if (attributes & PHX_FILESYSTEM_FAT_ENTRY_SYSTEM)
        nodeOut->attributes |= PHX_FILESYSTEM_ATTRIBUTE_SYSTEM;

    nodeOut->type = (attributes & PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY) ? PHX_FILESYSTEM_ENTRY_DIRECTORY : PHX_FILESYSTEM_ENTRY_FILE;

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_RemoveNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node)
{
    if (fs->readonly == PHX_TRUE) return PHX_ERROR_PERMISSION;
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* extra = node->extra;

    if (extra->startCluster == 0)
        return PHX_SUCCESS;

    PHX_u32 status;
    PHX_u32 cluster = extra->startCluster;

    while ((status = PHX_Filesystem_FAT_Cluster(data->version, cluster)) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
    {
        PHX_u32 next;
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &next)) != PHX_SUCCESS)
            return result;
        if ((result = PHX_Filesystem_FAT_WriteFAT(data, cluster, 0)) != PHX_SUCCESS)
            return result;
        data->freeClusterCount++;
        if (cluster < data->nextFreeCluster) data->nextFreeCluster = cluster;
        cluster = next;
    }

    if (status != PHX_FILESYSTEM_FAT_CLUSTER_EOC)
        return PHX_ERROR_FORMAT;

    return PHX_SUCCESS;
}

void PHX_Filesystem_FAT_CleanupNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node)
{
    fs->context->allocator.free(&fs->context->allocator, node->extra);
}
