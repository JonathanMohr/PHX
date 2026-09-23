#include "fat.h"

#include <embed/fat.h>


static struct PHX_Filesystem_Operations PHX_Filesystem_FAT_Operations = {
    PHX_Filesystem_FAT_ChangeBootsector,
    PHX_Filesystem_FAT_Destroy,
    
    PHX_Filesystem_FAT_GetRoot,
    PHX_Filesystem_FAT_GetNode,
    PHX_Filesystem_FAT_RemoveNode,
    PHX_Filesystem_FAT_CleanupNode,

    PHX_Filesystem_FAT_Dir_GetEntryCount,
    PHX_Filesystem_FAT_Dir_ReadEntry,
    PHX_Filesystem_FAT_Dir_LookupEntry,

    PHX_Filesystem_FAT_File_Read,
    PHX_Filesystem_FAT_File_Write,
    PHX_Filesystem_FAT_File_Seek,

    PHX_Filesystem_FAT_CreateNode,

    PHX_Filesystem_FAT_LinkEntry,
    PHX_Filesystem_FAT_UnlinkEntry,
    PHX_Filesystem_FAT_MoveEntry,

    PHX_Filesystem_FAT_CreateOpenNode,
    PHX_Filesystem_FAT_CloseOpenNode,
    PHX_Filesystem_FAT_ResetOpenNode
};


static PHX_Result PHX_Filesystem_FAT_OpenFilesystem(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs)
{
    PHX_Filesystem_FAT_Data* data = context->allocator.allocate(&context->allocator, sizeof(PHX_Filesystem_FAT_Data));
    if (!data)
        return PHX_ERROR_MEMORY;

    const PHX_BlockSize blocksForBootsector = (512 + device->blockSize - 1) / device->blockSize;

    PHX_Byte* sectorBuffer = context->allocator.allocate(&context->allocator, blockForBootsector);
    if (!sectorBuffer)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_ERROR_MEMORY;
    }

    if (device->read(device, sectorBuffer, 0, blocksForBootsector) != blocksForBootsector)
    {
        context->allocator.free(&context->allocator, data);
        context->allocator.free(&context->allocator, sectorBuffer);
        return PHX_ERROR_IO;
    }

    memcpy(data->bootsector, sectorBuffer, 512);

    context->allocator.free(&context->allocator, sectorBuffer);



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
