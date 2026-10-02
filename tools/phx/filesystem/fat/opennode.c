#include "fat.h"

PHX_Result PHX_Filesystem_FAT_CreateOpenNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNodeOut)
{
    PHX_Filesystem_FAT_Node_Extra* nodeExtra = node->extra;

    PHX_Filesystem_FAT_OpenNode_Extra* extra = fs->context->allocator.allocate(&fs->context->allocator, sizeof(PHX_Filesystem_FAT_OpenNode_Extra));
    if (!extra)
        return PHX_ERROR_MEMORY;

    extra->currentCluster = nodeExtra->startCluster;
    openNodeOut->pos = 0;

    openNodeOut->extra = extra;

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_CloseOpenNode(PHX_Filesystem* fs, PHX_Filesystem_OpenNode* openNode)
{
    PHX_Filesystem_FAT_OpenNode_Extra* extra = openNode->extra;

    fs->context->allocator.free(&fs->context->allocator, extra);

    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_ResetOpenNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNode)
{
    (void)fs;

    PHX_Filesystem_FAT_OpenNode_Extra* extra = openNode->extra;
    PHX_Filesystem_FAT_Node_Extra* nodeExtra = node->extra;

    extra->currentCluster = nodeExtra->startCluster;
    openNode->pos = 0;

    return PHX_SUCCESS;
}
