#include "fat.h"
#include "filesystem/filesystem.h"
#include "types.h"

PHX_Result PHX_Filesystem_FAT_Dir_GetEntryCount(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_u64* entryCountOut)
{
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* extra = dir->extra;

    PHX_u64 entryCount = 0;

    PHX_u32 status;
    PHX_u32 cluster = extra->startCluster;

    if (dir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32)
    {
        PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];

        for (PHX_u16 i = 0; i < data->specific.fat12_16.rootDirEntryCount; i++)
        {
            if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, i, 1, entry)) != PHX_SUCCESS)
                return result;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED || read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) == PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE || read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) & PHX_FILESYSTEM_FAT_ENTRY_VOLUME_LABEL)
                continue;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
                break;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == '.' && (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1) == ' ' || (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1) == '.' && read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 2) == ' ')))
                continue;

            entryCount++;
        }

        *entryCountOut = entryCount;
        return PHX_SUCCESS;
    }

    if (cluster == 0)
    {
        *entryCountOut = 0;
        return PHX_SUCCESS;
    }

    while ((status = PHX_Filesystem_FAT_Cluster(data->version, cluster)) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
    {
        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;
        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
            return result;

        PHX_Byte* entries = data->clusterBuffer;

        PHX_Bool eod = PHX_FALSE;
        for (PHX_u32 i = 0; i < data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE; i++)
        {
            PHX_Byte* entry = entries + i * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED || read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) == PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE || read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) & PHX_FILESYSTEM_FAT_ENTRY_VOLUME_LABEL)
                continue;

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            {
                status = PHX_FILESYSTEM_FAT_CLUSTER_EOC;
                eod = PHX_TRUE;
                break;
            }

            if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == '.' && (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1) == ' ' || (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1) == '.' && read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM + 2) == ' ')))
                continue;

            entryCount++;
        }

        if (eod == PHX_TRUE) break;
    }

    if (status != PHX_FILESYSTEM_FAT_CLUSTER_EOC)
        return PHX_ERROR_FORMAT;

    *entryCountOut = entryCount;
    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_Dir_ReadEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_OpenNode* openDir, PHX_Filesystem_Entry* entryOut)
{
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    // PHX_Filesystem_FAT_Node_Extra* dirExtra = dir->extra;
    PHX_Filesystem_FAT_OpenNode_Extra* openNodeExtra = openDir->extra;

    const PHX_Bool rootDirectory = (dir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32);

    PHX_u16 lfnChars[20 * 13];
    PHX_Byte lfnExpected = 0;
    PHX_Byte lfnChecksum = 0;
    PHX_Byte haveLfn = PHX_FALSE;

    while (1)
    {
        PHX_u32 entryCluster;
        PHX_u32 entryIndex;
        PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];

        if (rootDirectory)
        {
            if (openDir->pos >= data->specific.fat12_16.rootDirEntryCount)
                return PHX_ERROR_NOT_FOUND;

            entryIndex = (PHX_u32)openDir->pos;
            entryCluster = 0;

            if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, (PHX_u16)entryIndex, 1, entry)) != PHX_SUCCESS)
                return result;
        }
        else
        {
            const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;
            PHX_u32 indexInCluster = openDir->pos % entriesPerCluster;
            PHX_u32 offsetInCluster = indexInCluster * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            if (indexInCluster == 0 && openDir->pos != 0)
            {
                PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, openNodeExtra->currentCluster);
                if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                    return PHX_ERROR_FORMAT;

                PHX_u32 nextCluster;
                if ((result = PHX_Filesystem_FAT_ReadFAT(data, openNodeExtra->currentCluster, &nextCluster)) != PHX_SUCCESS)
                    return result;

                PHX_u32 nextStatus = PHX_Filesystem_FAT_Cluster(data->version, nextCluster);
                if (nextStatus == PHX_FILESYSTEM_FAT_CLUSTER_EOC)
                    return PHX_ERROR_NOT_FOUND;
                if (nextStatus != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                    return PHX_ERROR_FORMAT;

                openNodeExtra->currentCluster = nextCluster;
            }

            PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, openNodeExtra->currentCluster);
            if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                return PHX_ERROR_FORMAT;

            const PHX_u32 sectorOffset = offsetInCluster / data->bytesPerSector;
            const PHX_u32 offsetInSector = offsetInCluster % data->bytesPerSector;

            if (data->usedDevice->read(data->usedDevice, data->buffer, PHX_Filesystem_FAT_GetClusterStart(data, openNodeExtra->currentCluster) + sectorOffset, 1) != 1)
                return PHX_ERROR_IO;

            entryCluster = openNodeExtra->currentCluster;
            entryIndex = indexInCluster;
            memcpy(&entry, data->buffer + offsetInSector, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
        }

        openDir->pos++;

        if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            return PHX_ERROR_NOT_FOUND;

        if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED)
        {
            haveLfn = PHX_FALSE;
            continue;
        }

        if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) == PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE)
        {
            if (data->readWithLFN != PHX_TRUE) continue;

            const PHX_Byte order = read_u8(entry + PHX_FILESYSTEM_FAT_LFNENT_ORD);

            const PHX_Byte sequence = order & 0x1F;
            const PHX_Bool isLast = (order & 0x40) ? PHX_TRUE : PHX_FALSE;
            
            if (sequence == 0 || sequence > 20)
            {
                haveLfn = PHX_FALSE;
                continue;
            }

            if (isLast == PHX_TRUE)
            {
                memset(lfnChars, 0, sizeof(lfnChars));
                lfnExpected = sequence;
                lfnChecksum = read_u8(entry + PHX_FILESYSTEM_FAT_LFNENT_CHE);
                haveLfn = PHX_TRUE;
            }
            else if (haveLfn != PHX_TRUE || sequence != lfnExpected - 1 || read_u8(entry + PHX_FILESYSTEM_FAT_LFNENT_CHE) != lfnChecksum)
            {
                haveLfn = PHX_FALSE;
                continue;
            }
            else
                lfnExpected = sequence;

            PHX_u16 chars[13];
            PHX_Filesystem_FAT_LFN_ExtractChars(entry, chars);
            memcpy(&lfnChars[(sequence - 1) * 13], chars, sizeof(chars));

            continue;
        }

        if (read_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) & PHX_FILESYSTEM_FAT_ENTRY_VOLUME_LABEL)
        {
            haveLfn = PHX_FALSE;
            continue;
        }

        {
            const PHX_Byte* shortNameRaw = entry + PHX_FILESYSTEM_FAT_DIRENT_NAM;

            const PHX_Bool isDot = (shortNameRaw[0] == '.' && shortNameRaw[1] == ' ') ? PHX_TRUE : PHX_FALSE;
            const PHX_Bool isDotDot = (shortNameRaw[0] == '.' && shortNameRaw[1] == '.' && shortNameRaw[2] == ' ') ? PHX_TRUE : PHX_FALSE;

            if (isDot == PHX_TRUE || isDotDot == PHX_TRUE)
            {
                haveLfn = PHX_FALSE;
                continue;
            }
        }

        const PHX_Bool useLfn = (haveLfn == PHX_TRUE && lfnExpected == 1 && PHX_Filesystem_FAT_LFN_Checksum(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == lfnChecksum) ? PHX_TRUE : PHX_FALSE;
    
        if (useLfn == PHX_TRUE)
        {
            PHX_u32 written = PHX_Filesystem_FAT_UTF16_To_UTF8(lfnChars, 20 * 13, entryOut->name, sizeof(entryOut->name) - 1);
            entryOut->name[written] = '\0';
        }
        else
        {
            char shortName[13];
            PHX_Filesystem_FAT_BuildShortName(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, shortName);
            PHX_u32 len = 0;
            while (shortName[len] != '\0' && len < sizeof(entryOut->name) - 1)
            {
                entryOut->name[len] = shortName[len];
                len++;
            }
            entryOut->name[len] = '\0';
        }
        
        haveLfn = PHX_FALSE;

        //PHX_u32 firstCluster = read_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCL);
        //if (data->version == PHX_FILESYSTEM_FAT_32)
        //    firstCluster |= (PHX_u32)read_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCH) << 16;

        entryOut->node = ((PHX_u64)entryCluster << 32) | entryIndex;

        return PHX_SUCCESS;
    }
}

PHX_Result PHX_Filesystem_FAT_Dir_LookupEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Entry* entryOut)
{
    PHX_Result result;

    PHX_Filesystem_Entry entry;
    PHX_Filesystem_OpenNode dirOpenNode;
    if ((result = PHX_Filesystem_FAT_CreateOpenNode(fs, dir, &dirOpenNode)) != PHX_SUCCESS)
        return result;

    while (1)
    {
        if ((result = PHX_Filesystem_FAT_Dir_ReadEntry(fs, dir, &dirOpenNode, &entry)) != PHX_SUCCESS)
            break;

        if (PHX_Filesystem_FAT_NameEquals(entry.name, name) == PHX_TRUE)
            break;
    }

    (void)PHX_Filesystem_FAT_CloseOpenNode(fs, &dirOpenNode);

    if (result != PHX_SUCCESS) return result;

    memcpy(entryOut, &entry, sizeof(PHX_Filesystem_Entry));
    return PHX_SUCCESS;
}
