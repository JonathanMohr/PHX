#include "fat.h"
#include "filesystem/fat/fat.h"
#include "filesystem/filesystem.h"
#include "types.h"

PHX_Result PHX_Filesystem_FAT_CreateNode(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_Entry_Type type, PHX_Filesystem_Entry_Attribute attributes, const char* name, PHX_Filesystem_Node* nodeOut)
{
    PHX_Result result;
    PHX_Filesystem_FAT_Data* data = fs->data;

    PHX_Filesystem_FAT_Node_Extra* dirExtra = dir->extra;
    PHX_Filesystem_FAT_Node_Extra* newExtra = nodeOut ? fs->context->allocator.allocate(&fs->context->allocator, sizeof(PHX_Filesystem_FAT_Node_Extra)) : PHX_NULL;
    if (nodeOut && !newExtra)
        return PHX_ERROR_MEMORY;

    const char* namePtr = name;
    while (*namePtr)
        namePtr++;

    if ((namePtr - name) > 255)
    {
        if (newExtra) fs->context->allocator.free(&fs->context->allocator, newExtra);
        return PHX_ERROR_NAME_TOO_LONG;
    }

    PHX_u16 utf16Name[20 * 13];
    PHX_u32 utf16Count = PHX_Filesystem_FAT_UTF8_To_UTF16(name, (PHX_u32)(namePtr - name), utf16Name, 255);
    utf16Name[utf16Count++] = 0;

    namePtr = name;
    const char* lastPoint = PHX_NULL;
    while (namePtr)
    {
        if (*namePtr == '.') lastPoint = namePtr;
        namePtr++;
    }

    char firstChars[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    char ext[3] = {' ', ' ', ' '};

    PHX_Filesystem_FAT_GetShortNameCharacters(name, lastPoint, 8, firstChars);
    if (lastPoint)
        PHX_Filesystem_FAT_GetShortNameCharacters(lastPoint + 1, PHX_NULL, 3, ext);

    char hash[4];
    PHX_u32 rawHash = PHX_Filesystem_FAT_HashName(name);
    PHX_Filesystem_FAT_HashToChars(rawHash, hash);

    char shortName[11];
    if ((result = PHX_Filesystem_FAT_GenerateShortName(fs, dir, firstChars, hash, ext, shortName)) != PHX_SUCCESS)
    {
        if (newExtra) fs->context->allocator.free(&fs->context->allocator, newExtra);
        return result;
    }

    
    const PHX_u32 lfnSlotCount = (utf16Count + 13 - 1) / 13;
    const PHX_u32 totalEntries = lfnSlotCount + 1;
    const PHX_Byte lfnChecksum = PHX_Filesystem_FAT_LFN_Checksum((PHX_Byte*)shortName);

    const PHX_Bool isDir = (type == PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY) ? PHX_TRUE : PHX_FALSE;
    PHX_u32 cluster = 0;


    PHX_u32 entryCluster;
    PHX_u32 entryIndex;
    PHX_u32 mainEntryCluster;
    PHX_u32 mainEntryIndex;
    // TODO: FindFreeEntrySlots

    if (isDir)
    {
        if ((result = PHX_Filesystem_FAT_FindFreeClusters(data, 1, &cluster)) != PHX_SUCCESS)
        {
            if (newExtra) fs->context->allocator.free(&fs->context->allocator, newExtra);
            return result;
        }

        memset(data->clusterBuffer, 0, data->bytesPerCluster);

        PHX_Byte* dot = data->clusterBuffer;
        PHX_Byte* dotdot = dot + PHX_FILESYSTEM_FAT_DIRENT_SIZE;

        *(dot + PHX_FILESYSTEM_FAT_DIRENT_NAM) = '.';
        memset(dot + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1, ' ', 10);
        write_u8(dot + PHX_FILESYSTEM_FAT_DIRENT_ATR, PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY);
        write_u8(dot + PHX_FILESYSTEM_FAT_DIRENT_RES, 0);
        write_u8(dot + PHX_FILESYSTEM_FAT_DIRENT_CTT, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_CRT, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_CRD, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_LAD, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_FCH, (data->version == PHX_FILESYSTEM_FAT_32) ? (PHX_u16)(cluster >> 16) : 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_LMT, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_LMD, 0);
        write_u16(dot + PHX_FILESYSTEM_FAT_DIRENT_FCL, (PHX_u16)(cluster & 0xFFFF));
        write_u32(dot + PHX_FILESYSTEM_FAT_DIRENT_FCL, 0);

        // TODO: Check
        PHX_u32 parentCluster = dirExtra->startCluster;
        if (data->version == PHX_FILESYSTEM_FAT_32 && parentCluster == data->specific.fat32.rootDirCluster)
            parentCluster = 0;

        *(dotdot + PHX_FILESYSTEM_FAT_DIRENT_NAM) = '.';
        memset(dotdot + PHX_FILESYSTEM_FAT_DIRENT_NAM + 1, ' ', 10);
        write_u8(dotdot + PHX_FILESYSTEM_FAT_DIRENT_ATR, PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY);
        write_u8(dotdot + PHX_FILESYSTEM_FAT_DIRENT_RES, 0);
        write_u8(dotdot + PHX_FILESYSTEM_FAT_DIRENT_CTT, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_CRT, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_CRD, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_LAD, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_FCH, (data->version == PHX_FILESYSTEM_FAT_32) ? (PHX_u16)(parentCluster >> 16) : 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_LMT, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_LMD, 0);
        write_u16(dotdot + PHX_FILESYSTEM_FAT_DIRENT_FCL, (PHX_u16)(parentCluster & 0xFFFF));
        write_u32(dotdot + PHX_FILESYSTEM_FAT_DIRENT_FCL, 0);

        if (data->usedDevice->write(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
        {
            if (newExtra) fs->context->allocator.free(&fs->context->allocator, newExtra);
            return PHX_ERROR_IO;
        }
    }


    PHX_Byte entries[21][PHX_FILESYSTEM_FAT_DIRENT_SIZE];
    for (PHX_u32 i = 0; i < totalEntries; i++)
    {
        if (i < totalEntries - 1)
        {
            // TODO: Fill LFN entry
        }
        else
        {
            PHX_Byte* entry = entries[i];

            PHX_Byte attribute = 0;
            if (attributes & PHX_FILESYSTEM_ATTRIBUTE_READONLY)
                attribute |= PHX_FILESYSTEM_FAT_ENTRY_READONLY;
            if (attributes & PHX_FILESYSTEM_ATTRIBUTE_HIDDEN)
                attribute |= PHX_FILESYSTEM_FAT_ENTRY_HIDDEN;
            if (attributes & PHX_FILESYSTEM_ATTRIBUTE_SYSTEM)
                attribute |= PHX_FILESYSTEM_FAT_ENTRY_SYSTEM;

            if (type == PHX_FILESYSTEM_ENTRY_DIRECTORY)
                attribute |= PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY;

            memcpy(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, shortName, 8);
            memcpy(entry + PHX_FILESYSTEM_FAT_DIRENT_EXT, shortName + 8, 3);

            write_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR, attribute);
            write_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_RES, 0);
            write_u8(entry + PHX_FILESYSTEM_FAT_DIRENT_CTT, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_CRT, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_CRD, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_LAD, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCH, (data->version == PHX_FILESYSTEM_FAT_32) ? (PHX_u16)(cluster >> 16) : 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_LMT, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_LMD, 0);
            write_u16(entry + PHX_FILESYSTEM_FAT_DIRENT_FCL, (PHX_u16)(cluster & 0xFFFF));
            write_u32(entry + PHX_FILESYSTEM_FAT_DIRENT_FCL, 0);
        }
    }

    if ((result = PHX_Filesystem_FAT_WriteEntries(fs, entryCluster, entryIndex, entries, totalEntries)) != PHX_SUCCESS)
    {
        if (newExtra) fs->context->allocator.free(&fs->context->allocator, newExtra);
        return result;
    }


    if (nodeOut)
    {
        newExtra->startCluster = cluster;

        nodeOut->number = ((PHX_u64)mainEntryCluster << 32) | mainEntryIndex;
        nodeOut->size = (isDir == PHX_TRUE) ? data->bytesPerCluster : 0;
        nodeOut->referenceCount = 1;

        nodeOut->extra = newExtra;

        nodeOut->attributes = attributes;
        nodeOut->type = type;
    }

    return PHX_SUCCESS;
}
