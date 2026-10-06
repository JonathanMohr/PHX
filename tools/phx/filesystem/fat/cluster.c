#include "fat.h"
#include "result.h"

PHX_u32 PHX_Filesystem_FAT_Cluster(PHX_Filesystem_FAT_Version version, PHX_u32 cluster)
{
    switch (version)
    {
        case PHX_FILESYSTEM_FAT_12:
            if (cluster == 0x000)
                return PHX_FILESYSTEM_FAT_CLUSTER_FREE;
            if (cluster == 0x001)
                return PHX_FILESYSTEM_FAT_CLUSTER_ERROR;
            if (cluster <  0xFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_NORMAL;
            if (cluster == 0xFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_BAD;
            if (cluster <= 0xFFF)
                return PHX_FILESYSTEM_FAT_CLUSTER_EOC;
            break;

        case PHX_FILESYSTEM_FAT_16:
            if (cluster == 0x0000)
                return PHX_FILESYSTEM_FAT_CLUSTER_FREE;
            if (cluster == 0x0001)
                return PHX_FILESYSTEM_FAT_CLUSTER_ERROR;
            if (cluster <  0xFFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_NORMAL;
            if (cluster == 0xFFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_BAD;
            if (cluster <= 0xFFFF)
                return PHX_FILESYSTEM_FAT_CLUSTER_EOC;
            break;

        case PHX_FILESYSTEM_FAT_32:
            if (cluster == 0x00000000)
                return PHX_FILESYSTEM_FAT_CLUSTER_FREE;
            if (cluster == 0x00000001)
                return PHX_FILESYSTEM_FAT_CLUSTER_ERROR;
            if (cluster <  0x0FFFFFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_NORMAL;
            if (cluster == 0x0FFFFFF7)
                return PHX_FILESYSTEM_FAT_CLUSTER_BAD;
            if (cluster <= 0x0FFFFFFF)
                return PHX_FILESYSTEM_FAT_CLUSTER_EOC;
            break;
    }

    return PHX_FILESYSTEM_FAT_CLUSTER_ERROR;
}

PHX_Result PHX_Filesystem_FAT_ReadFAT(PHX_Filesystem_FAT_Data* data, PHX_u32 cluster, PHX_u32* outValue)
{
    if (cluster >= data->totalClusters + 2)
        return PHX_ERROR_INTERNAL;

    PHX_u64 fatIndex;
    PHX_Byte entrySize;

    switch (data->version)
    {
        case PHX_FILESYSTEM_FAT_12:
            fatIndex = (PHX_u64)cluster * 3 / 2;
            entrySize = 2;
            break;

        case PHX_FILESYSTEM_FAT_16:
            fatIndex = (PHX_u64)cluster * 2;
            entrySize = 2;
            break;

        case PHX_FILESYSTEM_FAT_32:
            fatIndex = (PHX_u64)cluster * 4;
            entrySize = 4;
            break;
    }

    PHX_u64 fatStartSector = (PHX_u64)data->fatSector;
    if (data->activeFat != PHX_FILESYSTEM_FAT_ACTIVE_ALL)
        fatStartSector += (PHX_u64)(data->fatSize / data->fatCount) * (PHX_u64)data->activeFat;

    PHX_u64 currentSector = fatStartSector + fatIndex / (PHX_u64)data->bytesPerSector;
    const PHX_u16 offsetInSector = (PHX_u16)(fatIndex % (PHX_u64)data->bytesPerSector);

    if (data->usedDevice->read(data->usedDevice, data->buffer, currentSector, 1) != 1)
        return PHX_ERROR_IO;

    PHX_Byte entryBytes[4];
    for (PHX_Byte i = 0; i < entrySize; i++)
    {
        if (offsetInSector + i == data->bytesPerSector)
        {
            currentSector++;
            if (data->usedDevice->read(data->usedDevice, data->buffer, currentSector, 1) != 1)
                return PHX_ERROR_IO;
        }
        entryBytes[i] = data->buffer[(offsetInSector + i) % data->bytesPerSector];
    }

    switch (data->version)
    {
        case PHX_FILESYSTEM_FAT_12:
        {
            const PHX_u16 rawValue = PHX_Filesystem_FAT_Read_u16(entryBytes);
            *outValue = ((cluster & 1) != 0) ? (rawValue >> 4) : (rawValue & 0x0FFF);
            break;
        }

        case PHX_FILESYSTEM_FAT_16:
            *outValue = PHX_Filesystem_FAT_Read_u16(entryBytes);
            break;

        case PHX_FILESYSTEM_FAT_32:
            *outValue = PHX_Filesystem_FAT_Read_u32(entryBytes) & 0x0FFFFFFF;
            break;
    }

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_WriteFAT(PHX_Filesystem_FAT_Data* data, PHX_u32 cluster, PHX_u32 value)
{
    if (cluster >= data->totalClusters + 2)
        return PHX_ERROR_INTERNAL;

    PHX_u64 fatIndex;

    switch (data->version)
    {
        case PHX_FILESYSTEM_FAT_12:
            fatIndex = (PHX_u64)cluster * 3 / 2;
            break;

        case PHX_FILESYSTEM_FAT_16:
            fatIndex = (PHX_u64)cluster * 2;
            break;

        case PHX_FILESYSTEM_FAT_32:
            fatIndex = (PHX_u64)cluster * 4;
            break;
    }

    const PHX_u64 singleFatSize = (PHX_u64)(data->fatSize / data->fatCount);

    PHX_u64 fatStartSector = (PHX_u64)data->fatSector;
    if (data->activeFat != PHX_FILESYSTEM_FAT_ACTIVE_ALL)
        fatStartSector += singleFatSize * (PHX_u64)data->activeFat;

    PHX_u64 currentSector = fatStartSector + fatIndex / (PHX_u64)data->bytesPerSector;
    PHX_u16 offsetInSector = (PHX_u16)(fatIndex % (PHX_u64)data->bytesPerSector);

    const PHX_Byte copyCount = (data->activeFat == PHX_FILESYSTEM_FAT_ACTIVE_ALL) ? data->fatCount : 1;

    if (data->usedDevice->read(data->usedDevice, data->buffer, currentSector, 1) != 1)
        return PHX_ERROR_IO;

    switch (data->version)
    {
        case PHX_FILESYSTEM_FAT_12:
        {
            const PHX_Bool isOddCluster = (cluster & 1) != 0;

            if (isOddCluster)
                data->buffer[offsetInSector] = (PHX_Byte)((data->buffer[offsetInSector] & 0x0F) | ((value & 0x0F) << 4));
            else
                data->buffer[offsetInSector] = (PHX_Byte)(value & 0xFF);

            if ((PHX_u32)offsetInSector + 1 == data->bytesPerSector)
            {
                for (PHX_Byte copy = copyCount; copy > 0; copy--)
                {
                    if (data->usedDevice->write(data->usedDevice, data->buffer, currentSector + (PHX_u64)(copy - 1) * singleFatSize, 1) != 1)
                        return PHX_ERROR_IO;
                }

                currentSector++;
                if (data->usedDevice->read(data->usedDevice, data->buffer, currentSector, 1) != 1)
                    return PHX_ERROR_IO;
                offsetInSector = 0;
            }
            else
                offsetInSector++;

            if (isOddCluster)
                data->buffer[offsetInSector] = (PHX_Byte)((value >> 4) & 0xFF);
            else
                data->buffer[offsetInSector] = (PHX_Byte)((data->buffer[offsetInSector] & 0xF0) | ((value >> 8) & 0x0F));
            break;
        }

        case PHX_FILESYSTEM_FAT_16:
            write_u16(data->buffer + offsetInSector, (PHX_u16)value);
            break;

        case PHX_FILESYSTEM_FAT_32:
            write_u32(data->buffer + offsetInSector, (value & 0x0FFFFFFF) | (PHX_Filesystem_FAT_Read_u32(data->buffer + offsetInSector) & 0xF0000000));
            break;
    }

    for (PHX_Byte copy = copyCount; copy > 0; copy--)
    {
        if (data->usedDevice->write(data->usedDevice, data->buffer, currentSector + (PHX_u64)(copy - 1) * singleFatSize, 1) != 1)
            return PHX_ERROR_IO;
    }

    return PHX_SUCCESS;
}


PHX_Result PHX_Filesystem_FAT_FindFreeClusters(PHX_Filesystem_FAT_Data* data, PHX_u32 count, PHX_u32* outFirstCluster)
{
    if (count == 0)
        return PHX_ERROR_INTERNAL;
    PHX_Result result;

    const PHX_u32 maxCluster = data->dataSize / data->sectorsPerCluster + 1;

    PHX_u32 firstCluster = 0;
    PHX_u32 previousCluster = 0;
    PHX_u32 found = 0;

    PHX_u32 cluster = (data->nextFreeCluster == 0xFFFFFFFF) ? 0 : data->nextFreeCluster;
    PHX_u32 startCluster = cluster;
    PHX_Bool wrapped = PHX_FALSE;

    while (found < count)
    {
        if (cluster > maxCluster)
        {
            if (wrapped == PHX_TRUE)
                break;
            cluster = 2;
            wrapped = PHX_TRUE;
            continue;
        }

        if (wrapped == PHX_TRUE && cluster == startCluster)
            break;

        PHX_u32 entry;
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &entry)) != PHX_SUCCESS)
            goto rollback;

        PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, entry);
        if (status == PHX_FILESYSTEM_FAT_CLUSTER_ERROR)
        {
            result = PHX_ERROR_INTERNAL;
            goto rollback;
        }

        if (status == PHX_FILESYSTEM_FAT_CLUSTER_FREE)
        {
            if (found == 0)
                firstCluster = cluster;
            else
            {
                if ((result = PHX_Filesystem_FAT_WriteFAT(data, previousCluster, cluster)) != PHX_SUCCESS)
                    goto rollback;
                data->freeClusterCount--;
            }

            previousCluster = cluster;
            found++;
        }

        cluster++;
    }

    if (found < count)
    {
        result = PHX_ERROR_OUT_OF_SPACE;
        goto rollback;
    }

    if ((result = PHX_Filesystem_FAT_WriteFAT(data, previousCluster, PHX_FILESYSTEM_FAT_CLUSTER_VALUE_EOC)) != PHX_SUCCESS)
        goto rollback;

    data->freeClusterCount--;

    data->nextFreeCluster = previousCluster + 1;
    if (data->nextFreeCluster > maxCluster)
        data->nextFreeCluster = 2;

    *outFirstCluster = firstCluster;
    return PHX_SUCCESS;

rollback:
    (void)0;
    
    PHX_u32 current = firstCluster;;
    for (PHX_u32 index = 1; index < found; index++)
    {
        PHX_u32 next;
        if (PHX_Filesystem_FAT_ReadFAT(data, current, &next) != PHX_SUCCESS)
            break;
        if (PHX_Filesystem_FAT_WriteFAT(data, current, 0) != PHX_SUCCESS)
            break;
        data->freeClusterCount++;
        current = next;
    }

    if (found > 0)
        (void)PHX_Filesystem_FAT_WriteFAT(data, previousCluster, 0);

    return result;
}

PHX_Result PHX_Filesystem_FAT_AppendClusters(PHX_Filesystem_FAT_Data* data, PHX_u32 lastCluster, PHX_u32 count, PHX_u32* firstNewClusterOut)
{
    PHX_Result result;

    PHX_u32 firstNew;
    if ((result = PHX_Filesystem_FAT_FindFreeClusters(data, count, &firstNew)) != PHX_SUCCESS)
        return result;

    PHX_u32 cluster = firstNew;
    for (PHX_u32 i = 0; i < count; i++)
    {
        memset(data->clusterBuffer, 0, data->bytesPerCluster);
        if (data->usedDevice->write(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;

        if (i + 1 < count)
        {
            if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
                return result;
        }
    }

    if (lastCluster != 0 && (result = PHX_Filesystem_FAT_WriteFAT(data, lastCluster, firstNew)) != PHX_SUCCESS)
        return result;

    *firstNewClusterOut = firstNew;
    return PHX_SUCCESS;
}
