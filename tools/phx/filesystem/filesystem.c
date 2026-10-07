#include "filesystem.h"

#include "fat/fat.h"

PHX_Filesystem_Interface* PHX_Filesystem_Interfaces[] = {
    &PHX_Filesystem_FAT_Interface
};

PHX_Size PHX_Filesystem_InterfaceCount = sizeof(PHX_Filesystem_Interfaces) / sizeof(PHX_Filesystem_Interfaces[0]);


PHX_Result PHX_Filesystem_GetEntry(PHX_Filesystem* fs, const char* path, PHX_Filesystem_Node* wd, PHX_Filesystem_Node* nodeOut)
{
    PHX_Result result;

    PHX_Bool useNewNode = PHX_FALSE;

    if (*path == '/' || !wd)
    {
        if ((result = fs->ops->getNode(fs, fs->ops->rootNodeNumber, nodeOut)) != PHX_SUCCESS)
            return result;

        wd = nodeOut;
        useNewNode = PHX_TRUE;
    }

    while (*path == '/')
        path++;

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

    PHX_Size nameOffset;
    char* nameBuffer = fs->context->allocator.allocate(&fs->context->allocator, longestPart + 1);
    if (!nameBuffer)
    {
        if (useNewNode == PHX_TRUE) fs->ops->cleanupNode(fs, nodeOut);
        return PHX_ERROR_MEMORY;
    }

    pathPtr = path;
    while (*pathPtr)
    {
        nameOffset = 0;
        while (*pathPtr && *pathPtr != '/')
            nameBuffer[nameOffset++] = *pathPtr++;
        nameBuffer[nameOffset] = '\0';

        while (*pathPtr == '/')
            pathPtr++;

        const PHX_Size nameLen = nameOffset;
        if (nameLen == 0)
            continue;

        if (nameLen == 1 && nameBuffer[0] == '.')
            continue;

        if (nameLen == 2 && nameBuffer[0] == '.' && nameBuffer[1] == '.')
        {
            // TODO: handle ..
            continue;
        }

        PHX_Filesystem_Entry entry;
        if ((result = fs->ops->dir_lookupEntry(fs, wd, nameBuffer, &entry)) != PHX_SUCCESS)
        {
            if (useNewNode == PHX_TRUE) fs->ops->cleanupNode(fs, nodeOut);
            return result;
        }

        if ((result = fs->ops->getNode(fs, entry.node, nodeOut)) != PHX_SUCCESS)
        {
            if (useNewNode == PHX_TRUE) fs->ops->cleanupNode(fs, nodeOut);
            return result;
        }

        wd = nodeOut;
        useNewNode = PHX_TRUE;
    }

    return PHX_SUCCESS;
}
