#include "fat.h"

PHX_Result PHX_Filesystem_FAT_MoveEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* srcDir, const char* srcName, PHX_Filesystem_Node* dstDir, const char* dstName, PHX_Filesystem_NodeNumber* newNumberOut)
{
    if (fs->readonly == PHX_TRUE) return PHX_ERROR_PERMISSION;
    PHX_Result result;

    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* srcDirExtra = srcDir->extra;

    const PHX_Bool srcRootDirectory = (srcDir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32);

    const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;

    PHX_u32 cluster = srcDirExtra->startCluster;
    PHX_u32 pos = 0;

    PHX_u32 lfnStartCluster = 0;
    PHX_u32 lfnStartIndex = 0;
    PHX_u32 lfnCount = 0;

    PHX_u16 lfnChars[20 * 13];
    PHX_Byte lfnExpected = 0;
    PHX_Byte lfnChecksum = 0;
    PHX_Byte haveLfn = PHX_FALSE;

    PHX_Bool found = PHX_FALSE;
    PHX_Byte mainEntry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];
    PHX_u32 srcStartCluster = 0;
    PHX_u32 srcStartIndex = 0;
    PHX_u32 srcTotalCount = 0;

    const char* namePtr = dstName;
    while (*namePtr)
        namePtr++;

    if ((namePtr - dstName) > 255)
        return PHX_ERROR_NAME_TOO_LONG;

    while (1)
    {
        PHX_u32 entryCluster;
        PHX_u32 entryIndex;
        PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];

        if (srcRootDirectory)
        {
            if (pos >= data->specific.fat12_16.rootDirEntryCount)
                break;

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
            break;

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

        if (PHX_Filesystem_FAT_NameEquals(candidateName, srcName))
        {
            found = PHX_TRUE;
            memcpy(mainEntry, entry, PHX_FILESYSTEM_FAT_DIRENT_SIZE);

            if (useLfn == PHX_TRUE)
            {
                srcStartCluster = lfnStartCluster;
                srcStartIndex = lfnStartIndex;
                srcTotalCount = lfnCount + 1;
            }
            else
            {
                srcStartCluster = entryCluster;
                srcStartIndex = entryIndex;
                srcTotalCount = 1;
            }

            break;
        }

        haveLfn = PHX_FALSE;
    }

    if (found != PHX_TRUE)
        return PHX_ERROR_NOT_FOUND;


    PHX_u16 utf16Name[20 * 13];
    PHX_u32 utf16Count = PHX_Filesystem_FAT_UTF8_To_UTF16(dstName, (PHX_u32)(namePtr - dstName), utf16Name, 255);
    utf16Name[utf16Count++] = 0;

    namePtr = dstName;
    const char* lastPoint = PHX_NULL;
    while (*namePtr)
    {
        if (*namePtr == '.') lastPoint = namePtr;
        namePtr++;
    }

    char firstChars[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char ext[3] = {' ', ' ', ' '};

    PHX_Filesystem_FAT_GetShortNameCharacters(dstName, lastPoint, 8, firstChars);
    if (lastPoint)
        PHX_Filesystem_FAT_GetShortNameCharacters(lastPoint + 1, PHX_NULL, 3, ext);

    char hash[4];
    PHX_u32 rawHash = PHX_Filesystem_FAT_HashName(dstName);
    PHX_Filesystem_FAT_HashToChars(rawHash, hash);

    char shortName[11];
    if ((result = PHX_Filesystem_FAT_GenerateShortName(fs, dstDir, firstChars, hash, ext, shortName)) != PHX_SUCCESS)
        return result;

    const PHX_u32 lfnSlotCount = (utf16Count + 13 - 1) / 13;
    const PHX_u32 totalEntries = lfnSlotCount + 1;
    const PHX_Byte lfnChecksumNew = PHX_Filesystem_FAT_LFN_Checksum((PHX_Byte*)shortName);

    PHX_u32 entryCluster;
    PHX_u32 entryIndex;
    PHX_u32 mainEntryCluster;
    PHX_u32 mainEntryIndex;
    if ((result = PHX_Filesystem_FAT_FindFreeEntrySlots(fs, dstDir, totalEntries, &entryCluster, &entryIndex, &mainEntryCluster, &mainEntryIndex)) != PHX_SUCCESS)
        return result;

    PHX_Byte entries[21][PHX_FILESYSTEM_FAT_DIRENT_SIZE];
    for (PHX_u32 i = 0; i < totalEntries; i++)
    {
        if (i < totalEntries - 1)
        {
            PHX_Filesystem_FAT_LFN_BuildEntry(utf16Name, utf16Count, lfnSlotCount, i, lfnChecksumNew, entries[i]);
        }
        else
        {
            PHX_Byte* entry = entries[i];
            memcpy(entry, mainEntry, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
            memcpy(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, shortName, 8);
            memcpy(entry + PHX_FILESYSTEM_FAT_DIRENT_EXT, shortName + 8, 3);
        }
    }

    if ((result = PHX_Filesystem_FAT_WriteEntries(fs, entryCluster, entryIndex, entries, totalEntries)) != PHX_SUCCESS)
        return result;

    if ((read_u8(mainEntry + PHX_FILESYSTEM_FAT_DIRENT_ATR) & PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY) && srcDir->number != dstDir->number)
    {
        PHX_u32 movedCluster = read_u16(mainEntry + PHX_FILESYSTEM_FAT_DIRENT_FCL);
        if (data->version == PHX_FILESYSTEM_FAT_32)
            movedCluster |= (PHX_u32)read_u16(mainEntry + PHX_FILESYSTEM_FAT_DIRENT_FCH) << 16;

        PHX_u32 newParentCluster = ((PHX_Filesystem_FAT_Node_Extra*)dstDir->extra)->startCluster;
        if (data->version == PHX_FILESYSTEM_FAT_32 && newParentCluster == data->specific.fat32.rootDirCluster)
            newParentCluster = 0;

        if (movedCluster >= 2)
        {
            const PHX_BlockSize movedStart = PHX_Filesystem_FAT_GetClusterStart(data, movedCluster);
            if (data->usedDevice->read(data->usedDevice, data->buffer, movedStart, 1) != 1)
                return PHX_ERROR_IO;

            PHX_Byte* dotdot = data->buffer + PHX_FILESYSTEM_FAT_DIRENT_SIZE;
            write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_FCH, (data->version == PHX_FILESYSTEM_FAT_32) ? (PHX_u16)(newParentCluster >> 16) : 0);
            write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_FCL, (PHX_u16)(newParentCluster & 0xFFFF));

            if (data->usedDevice->write(data->usedDevice, data->buffer, movedStart, 1) != 1)
                return PHX_ERROR_IO;
        }
    }

    PHX_Byte deletedSlots[21][PHX_FILESYSTEM_FAT_DIRENT_SIZE];
    for (PHX_u32 i = 0; i < srcTotalCount; i++)
    {
        memset(&deletedSlots[i], 0, PHX_FILESYSTEM_FAT_DIRENT_SIZE);
        *(deletedSlots[i] + PHX_FILESYSTEM_FAT_DIRENT_NAM) = PHX_FILESYSTEM_FAT_ENTRY_DELETED;
    }

    if ((result = PHX_Filesystem_FAT_WriteEntries(fs, srcStartCluster, srcStartIndex, deletedSlots, srcTotalCount)) != PHX_SUCCESS)
        return result;

    *newNumberOut = ((PHX_u64)mainEntryCluster << 32) | mainEntryIndex;
    return PHX_SUCCESS;
}
