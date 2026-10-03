#include "fat.h"
#include "result.h"
#include "types.h"

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
        fatStartSector += (PHX_u64)data->fatSize * (PHX_u64)data->activeFat;

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

    PHX_u64 fatStartSector = (PHX_u64)data->fatSector;
    if (data->activeFat != PHX_FILESYSTEM_FAT_ACTIVE_ALL)
        fatStartSector += (PHX_u64)data->fatSize * (PHX_u64)data->activeFat;

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
                    if (data->usedDevice->write(data->usedDevice, data->buffer, currentSector + (PHX_u64)(copy - 1) * (PHX_u64)data->fatSize, 1) != 1)
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
        if (data->usedDevice->write(data->usedDevice, data->buffer, currentSector + (PHX_u64)(copy - 1) * (PHX_u64)data->fatSize, 1) != 1)
            return PHX_ERROR_IO;
    }

    return PHX_SUCCESS;
}
