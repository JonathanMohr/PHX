#include "fat.h"

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
        return PHX_ERROR_NAME_TOO_LONG;

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
        return result;

    
    const PHX_u32 lfnSlotCount = (utf16Count + 13 - 1) / 13;
    const PHX_u32 totalEntries = lfnSlotCount + 1;
    const PHX_Byte lfnChecksum = PHX_Filesystem_FAT_LFN_Checksum((PHX_Byte*)shortName);

    const PHX_Bool isDir = (type == PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY) ? PHX_TRUE : PHX_FALSE;
    PHX_u32 cluster = 0;


    PHX_u32 entryCluster;
    PHX_u32 entryIndex;
    PHX_u32 mainEntryCluster;
    PHX_u32 mainEntryIndex;
    // TODO
}
