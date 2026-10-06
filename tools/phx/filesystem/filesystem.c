#include "filesystem.h"

#include "fat/fat.h"

PHX_Filesystem_Interface* PHX_Filesystem_Interfaces[] = {
    &PHX_Filesystem_FAT_Interface
};

PHX_Size PHX_Filesystem_InterfaceCount = sizeof(PHX_Filesystem_Interfaces) / sizeof(PHX_Filesystem_Interfaces[0]);


PHX_Result PHX_Filesystem_GetEntry(PHX_Filesystem* fs, const char* path, PHX_Filesystem_Entry* entryOut)
{
    // TODO: ..
    
    const char* pathPtr = path;
    PHX_Size longestPart = 0;
    PHX_Size currentRun = 0;
    while (*pathPtr)
    {
        if (*pathPtr == '/')
            currentRun = 0;
        else
            currentRun++;

        if (currentRun > longestPart)
            longestPart = currentRun;
        pathPtr++;
    }

    char* nameBuffer = fs->context->allocator.allocate(&fs->context->allocator, longestPart + 1);
    if (!nameBuffer)
        return PHX_ERROR_MEMORY;

    pathPtr = path;
    while (*pathPtr)
    {
        
    }
}
