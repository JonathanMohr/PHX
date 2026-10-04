#include "fat.h"
#include "types.h"

PHX_Result PHX_Filesystem_FAT_LinkEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Node* target)
{
    if (fs->readonly == PHX_TRUE) return PHX_ERROR_PERMISSION;
    (void)dir;
    (void)name;
    (void)target;
    return PHX_ERROR_NOT_SUPPORTED;
}

PHX_Result PHX_Filesystem_FAT_UnlinkEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Size* newReferenceCountOut)
{
    if (fs->readonly == PHX_TRUE) return PHX_ERROR_PERMISSION;
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* dirExtra = dir->extra;

    const PHX_Bool rootDirectory = (dir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32);

    const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;

    PHX_u32 cluster = dirExtra->startCluster;
    PHX_u32 pos = 0;

    PHX_u32 lfnStartCluster = 0;
    PHX_u32 lfnStartIndex = 0;
    PHX_u32 lfnCount = 0;

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
            if (pos >= data->specific.fat12_16.rootDirEntryCount)
                return PHX_ERROR_NOT_FOUND;

            entryIndex = pos;
            entryCluster = 0;

            if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, (PHX_u16)entryIndex, 1, entry)) != PHX_SUCCESS)
                return result;
        }
        else
        {
            PHX_u32 indexInCluster = pos % entriesPerCluster;
            PHX_u32 offsetInCluster = indexInCluster * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            if (indexInCluster == 0 && pos != 0)
            {
                PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, cluster);
                if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                    return PHX_ERROR_FORMAT;
                if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
                    return result;
            }

            PHX_u32 status = PHX_Filesystem_FAT_Cluster(data->version, cluster);
            if (status != PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
                return PHX_ERROR_FORMAT;

            const PHX_u32 sectorOffset = offsetInCluster / data->bytesPerSector;
            const PHX_u32 offsetInSector = offsetInCluster % data->bytesPerSector;

            if (data->usedDevice->read(data->usedDevice, data->buffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster) + sectorOffset, 1) != 1)
                return PHX_ERROR_IO;

            entryCluster = cluster;
            entryIndex = indexInCluster;
            memcpy(&entry, data->buffer + offsetInSector, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
        }

        pos++;

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
                lfnStartCluster = entryCluster;
                lfnStartIndex = entryIndex;
                lfnCount = 1;
            }
            else if (haveLfn != PHX_TRUE || sequence != lfnExpected - 1 || read_u8(entry + PHX_FILESYSTEM_FAT_LFNENT_CHE) != lfnChecksum)
            {
                haveLfn = PHX_FALSE;
                continue;
            }
            else
            {
                lfnExpected = sequence;
                lfnCount++;
            }

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

        const PHX_Bool useLfn = (haveLfn == PHX_TRUE && lfnExpected == 1 && PHX_Filesystem_FAT_LFN_Checksum(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == lfnChecksum) ? PHX_TRUE : PHX_FALSE;
    
        char candidateName[781];
        if (useLfn == PHX_TRUE)
        {
            PHX_u32 written = PHX_Filesystem_FAT_UTF16_To_UTF8(lfnChars, 20 * 13, candidateName, sizeof(candidateName) - 1);
            candidateName[written] = '\0';
        }
        else
        {
            PHX_Filesystem_FAT_BuildShortName(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, candidateName);
        }

        if (PHX_Filesystem_FAT_NameEquals(candidateName, name))
        {
            const PHX_u32 startCluster = (useLfn == PHX_TRUE) ? lfnStartCluster : entryCluster;
            const PHX_u32 startIndex = (useLfn == PHX_TRUE) ? lfnStartIndex : entryIndex;
            const PHX_u32 totalCount = (useLfn == PHX_TRUE) ? (lfnCount + 1) : 1;

            PHX_Byte deletedSlots[21][PHX_FILESYSTEM_FAT_DIRENT_SIZE];
            for (PHX_u32 i = 0; i < totalCount; i++)
            {
                memset(&deletedSlots[i], 0, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
                *(deletedSlots[i] + PHX_FILESYSTEM_FAT_DIRENT_NAM) = PHX_FILESYSTEM_FAT_ENTRY_DELETED;
            }

            if ((result = PHX_Filesystem_FAT_WriteEntries(fs, startCluster, startIndex, deletedSlots, totalCount)) != PHX_SUCCESS)
                return result;

            *newReferenceCountOut = 0;
            return PHX_SUCCESS;
        }

        haveLfn = PHX_FALSE;
    }
}
