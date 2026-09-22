#include "fat.h"

static PHX_Result PHX_Filesystem_FAT_OpenFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs)
{
    (void)context;
    (void)device;
    (void)outFs;
    return PHX_ERROR_INTERNAL;
}


static PHX_Result PHX_Filesystem_FAT_FormatFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs, const PHX_Byte* bootsector)
{
    (void)context;
    (void)device;
    (void)outFs;
    (void)bootsector;
    return PHX_ERROR_INTERNAL;
}


PHX_Filesystem_Interface PHX_Filesystem_FAT_Interface = {
    PHX_Filesystem_FAT_OpenFilesystem,
    PHX_Filesystem_FAT_FormatFilesystem,
    PHX_FILESYSTEM_FAT_TYPE,
    "FAT-FILESYSTEM-INTERFACE"
};
