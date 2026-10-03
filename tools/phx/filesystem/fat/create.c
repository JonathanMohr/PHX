#include "fat.h"

PHX_Result PHX_Filesystem_FAT_CreateNode(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_Entry_Type type, PHX_Filesystem_Entry_Attribute attributes, const char* name, PHX_Filesystem_Node* nodeOut)
{
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

    const char* lastPoint = PHX_NULL;
    namePtr = name;
    while (namePtr)
    {
        if (*namePtr == '.') lastPoint = namePtr;
        namePtr++;
    }

    char firstChars[2] = {'#', '#'};
    char ext[3] = {' ', ' ', ' '};

    // TODO
}
