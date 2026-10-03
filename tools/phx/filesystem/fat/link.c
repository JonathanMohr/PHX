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
    // TODO: Implement

    return PHX_ERROR_INTERNAL;
}
