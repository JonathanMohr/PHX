#include "device.h"

#include <base.h>

#include "raw.h"
#include "types.h"
#include "vdi.h"

PHX_Disk_Interface* PHX_Disk_Interfaces[] = {
    // TODO: &PHX_VDI_Interface,
    &PHX_RAW_Interface
};
PHX_Size PHX_Disk_InterfaceCount = sizeof(PHX_Disk_Interfaces) / sizeof(PHX_Disk_Interfaces[0]);


#define PHX_BLOCK_SIZE_TRANSFORM_TYPE "BLOCK-SIZE-TRANSFORM-DEVICE"

typedef enum
{
    PHX_BST_IDENTITY,
    PHX_BST_TARGET_MULTIPLE,
    PHX_BST_SOURCE_MULTIPLE,
    PHX_BST_GENERAL
} PHX_BlockSizeRelation;

#define PHX_BST_CHUNK_TARGET_BLOCKS 128

typedef struct PHX_BlockSizeTransform
{
    PHX_Context* context;
    PHX_BlockDevice* source;

    PHX_Byte* scratch;
    PHX_BlockSize scratchSourceBlocks;

    PHX_BlockSize targetBlockSize;
    PHX_BlockSize ratio;
    PHX_BlockSizeRelation relation;

    PHX_Bool ownsSource;
} PHX_BlockSizeTransform;

static inline PHX_BlockByteSize PHX_CeilDiv(PHX_BlockByteSize a, PHX_BlockByteSize b) { return (a + b - 1) / b; }


static PHX_BlockSize BST_Read_Identity(PHX_BlockSizeTransform* t, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    return t->source->read(t->source, buffer, block, count);
}

static PHX_BlockSize BST_Write_Identity(PHX_BlockSizeTransform* t, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    return t->source->write(t->source, buffer, block, count);
}


static PHX_BlockSize BST_Read_TargetMultiple(PHX_BlockSizeTransform* t, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize srcBlock = block * t->ratio;
    const PHX_BlockSize srcCount = count * t->ratio;
    const PHX_BlockSize srcRead = t->source->read(t->source, buffer, srcBlock, srcCount);
    return srcRead / t->ratio;
}

static PHX_BlockSize BST_Write_TargetMultiple(PHX_BlockSizeTransform* t, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize srcBlock = block * t->ratio;
    const PHX_BlockSize srcCount = count * t->ratio;
    const PHX_BlockSize srcWritten = t->source->write(t->source, buffer, srcBlock, srcCount);
    return srcWritten / t->ratio;
}


static PHX_BlockSize BST_Read_SourceMultiple(PHX_BlockSizeTransform* t, PHX_Byte* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize ratio = t->ratio;
    const PHX_BlockSize targetBlockSize = t->targetBlockSize;
    PHX_BlockSize done = 0;

    while (done < count)
    {
        const PHX_BlockSize remaining = count - done;
        const PHX_BlockSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_BlockSize targetBlock = block + done;

        const PHX_BlockSize firstSrcBlock = targetBlock / ratio;
        const PHX_BlockSize lastSrcBlock  = (targetBlock + chunk - 1) / ratio;
        const PHX_BlockSize srcBlockCount = lastSrcBlock - firstSrcBlock + 1;
        const PHX_BlockSize subOffset     = targetBlock % ratio;

        const PHX_BlockSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead == 0)
            return done;

        const PHX_BlockSize availableTargetBlocks = (srcRead * ratio) - subOffset;
        const PHX_BlockSize copyBlocks = (availableTargetBlocks < chunk) ? availableTargetBlocks : chunk;

        memcpy(buffer + done * targetBlockSize, t->scratch + subOffset * targetBlockSize, copyBlocks * targetBlockSize);

        done += copyBlocks;
        if (copyBlocks < chunk)
            break;
    }

    return done;
}

static PHX_BlockSize BST_Write_SourceMultiple(PHX_BlockSizeTransform* t, const PHX_Byte* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize ratio = t->ratio;
    const PHX_BlockSize targetBlockSize = t->targetBlockSize;
    PHX_BlockSize done = 0;

    while (done < count)
    {
        const PHX_BlockSize remaining = count - done;
        const PHX_BlockSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_BlockSize targetBlock = block + done;

        if (targetBlock % ratio == 0 && chunk % ratio == 0)
        {
            const PHX_BlockSize srcBlock = targetBlock / ratio;
            const PHX_BlockSize srcCount = chunk / ratio;
            const PHX_BlockSize srcWritten = t->source->write(t->source, buffer + done * targetBlockSize, srcBlock, srcCount);
            const PHX_BlockSize writtenTargetBlocks = srcWritten * ratio;
            done += writtenTargetBlocks;
            if (writtenTargetBlocks < chunk)
                break;
            continue;
        }

        const PHX_BlockSize firstSrcBlock = targetBlock / ratio;
        const PHX_BlockSize lastSrcBlock  = (targetBlock + chunk - 1) / ratio;
        const PHX_BlockSize srcBlockCount = lastSrcBlock - firstSrcBlock + 1;
        const PHX_BlockSize subOffset     = targetBlock % ratio;

        const PHX_BlockSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead != srcBlockCount)
            return done;

        memcpy(t->scratch + subOffset * targetBlockSize, buffer + done * targetBlockSize, chunk * targetBlockSize);

        const PHX_BlockSize srcWritten = t->source->write(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcWritten != srcBlockCount)
            return done;

        done += chunk;
    }

    return done;
}


static PHX_BlockSize BST_Read_General(PHX_BlockSizeTransform* t, PHX_Byte* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize targetBlockSize = t->targetBlockSize;
    const PHX_BlockSize sourceBlockSize = t->source->blockSize;
    PHX_BlockSize done = 0;

    while (done < count)
    {
        const PHX_BlockSize remaining = count - done;
        const PHX_BlockSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_BlockSize targetBlock = block + done;

        const PHX_BlockByteSize byteOffset = (PHX_BlockByteSize)targetBlock * targetBlockSize;
        const PHX_BlockByteSize byteCount  = (PHX_BlockByteSize)chunk * targetBlockSize;
        const PHX_BlockByteSize byteEnd    = byteOffset + byteCount;

        const PHX_BlockSize firstSrcBlock = (PHX_BlockSize)(byteOffset / sourceBlockSize);
        const PHX_BlockSize lastSrcBlock  = (PHX_BlockSize)PHX_CeilDiv(byteEnd, sourceBlockSize);
        const PHX_BlockSize srcBlockCount = lastSrcBlock - firstSrcBlock;
        const PHX_BlockByteSize relOffset = byteOffset - (PHX_BlockByteSize)firstSrcBlock * sourceBlockSize;

        const PHX_BlockSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        const PHX_BlockByteSize bytesRead = (PHX_BlockByteSize)srcRead * sourceBlockSize;

        const PHX_BlockByteSize usable = (bytesRead > relOffset) ? (bytesRead - relOffset) : 0;
        const PHX_BlockByteSize bytesToCopy = (usable < byteCount) ? usable : byteCount;

        if (bytesToCopy > 0)
            memcpy(buffer + done * targetBlockSize, t->scratch + relOffset, bytesToCopy);

        const PHX_BlockSize copyBlocks = bytesToCopy / targetBlockSize;
        done += copyBlocks;
        if (copyBlocks < chunk)
            break;
    }

    return done;
}

static PHX_BlockSize BST_Write_General(PHX_BlockSizeTransform* t, const PHX_Byte* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    const PHX_BlockSize targetBlockSize = t->targetBlockSize;
    const PHX_BlockSize sourceBlockSize = t->source->blockSize;
    PHX_BlockSize done = 0;

    while (done < count)
    {
        const PHX_BlockSize remaining = count - done;
        const PHX_BlockSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_BlockSize targetBlock = block + done;

        const PHX_BlockByteSize byteOffset = (PHX_BlockByteSize)targetBlock * targetBlockSize;
        const PHX_BlockByteSize byteCount  = (PHX_BlockByteSize)chunk * targetBlockSize;
        const PHX_BlockByteSize byteEnd    = byteOffset + byteCount;

        const PHX_BlockSize firstSrcBlock = (PHX_BlockSize)(byteOffset / sourceBlockSize);
        const PHX_BlockSize lastSrcBlock  = (PHX_BlockSize)PHX_CeilDiv(byteEnd, sourceBlockSize);
        const PHX_BlockSize srcBlockCount = lastSrcBlock - firstSrcBlock;
        const PHX_BlockByteSize relOffset = byteOffset - (PHX_BlockByteSize)firstSrcBlock * sourceBlockSize;

        if (relOffset == 0 && byteCount % sourceBlockSize == 0)
        {
            const PHX_BlockSize srcWritten = t->source->write(t->source, buffer + done * targetBlockSize, firstSrcBlock, srcBlockCount);
            const PHX_BlockByteSize writtenBytes = (PHX_BlockByteSize)srcWritten * sourceBlockSize;
            const PHX_BlockSize writtenBlocks = (PHX_BlockSize)(writtenBytes / targetBlockSize);
            done += writtenBlocks;
            if (writtenBlocks < chunk)
                break;
            continue;
        }

        const PHX_BlockSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead != srcBlockCount)
            return done;

        memcpy(t->scratch + relOffset, buffer + done * targetBlockSize, byteCount);

        const PHX_BlockSize srcWritten = t->source->write(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcWritten != srcBlockCount)
            return done;

        done += chunk;
    }

    return done;
}


static PHX_BlockSize BST_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_BlockSizeTransform* t = (PHX_BlockSizeTransform*)device->data;
    if (count == 0) return 0;
    switch (t->relation)
    {
        case PHX_BST_IDENTITY:        return BST_Read_Identity(t, buffer, block, count);
        case PHX_BST_TARGET_MULTIPLE: return BST_Read_TargetMultiple(t, buffer, block, count);
        case PHX_BST_SOURCE_MULTIPLE: return BST_Read_SourceMultiple(t, (PHX_Byte*)buffer, block, count);
        case PHX_BST_GENERAL:         return BST_Read_General(t, (PHX_Byte*)buffer, block, count);
    }
    return 0;
}

static PHX_BlockSize BST_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_BlockSizeTransform* t = (PHX_BlockSizeTransform*)device->data;
    if (device->readonly || count == 0) return 0;
    switch (t->relation)
    {
        case PHX_BST_IDENTITY:        return BST_Write_Identity(t, buffer, block, count);
        case PHX_BST_TARGET_MULTIPLE: return BST_Write_TargetMultiple(t, buffer, block, count);
        case PHX_BST_SOURCE_MULTIPLE: return BST_Write_SourceMultiple(t, (const PHX_Byte*)buffer, block, count);
        case PHX_BST_GENERAL:         return BST_Write_General(t, (const PHX_Byte*)buffer, block, count);
    }
    return 0;
}

static void BST_Close(PHX_BlockDevice* device)
{
    PHX_BlockSizeTransform* t = (PHX_BlockSizeTransform*)device->data;
    PHX_Context* context = t->context;
    if (t->ownsSource && t->source->close)
        t->source->close(t->source);
    if (t->scratch)
        context->allocator.free(&context->allocator, t->scratch);
    context->allocator.free(&context->allocator, t);
}


PHX_Bool PHX_BlockCountTransformDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_BlockSize targetBlockSize, PHX_Bool takeOwnership, PHX_BlockDevice* outDevice)
{
    if (targetBlockSize == 0 || device->blockSize == 0)
        return PHX_FALSE;

    PHX_BlockSizeTransform* t = (PHX_BlockSizeTransform*)context->allocator.allocate(&context->allocator, sizeof(PHX_BlockSizeTransform));
    if (!t)
        return PHX_FALSE;

    memset(t, 0, sizeof(PHX_BlockSizeTransform));
    t->context = context;
    t->source = device;
    t->ownsSource = takeOwnership;
    t->targetBlockSize = targetBlockSize;

    const PHX_BlockSize sourceBlockSize = device->blockSize;
    PHX_BlockSize outBlockCount;

    if (targetBlockSize == sourceBlockSize)
    {
        t->relation = PHX_BST_IDENTITY;
        outBlockCount = device->blockCount;
    }
    else if (targetBlockSize % sourceBlockSize == 0)
    {
        t->relation = PHX_BST_TARGET_MULTIPLE;
        t->ratio = targetBlockSize / sourceBlockSize;
        outBlockCount = device->blockCount / t->ratio;
    }
    else if (sourceBlockSize % targetBlockSize == 0)
    {
        t->relation = PHX_BST_SOURCE_MULTIPLE;
        t->ratio = sourceBlockSize / targetBlockSize;
        outBlockCount = device->blockCount * t->ratio;

        const PHX_BlockSize maxSrcBlocks = PHX_CeilDiv(PHX_BST_CHUNK_TARGET_BLOCKS, t->ratio) + 1;
        t->scratchSourceBlocks = maxSrcBlocks;
        t->scratch = (PHX_Byte*)context->allocator.allocate(&context->allocator, maxSrcBlocks * sourceBlockSize);
        if (!t->scratch)
        {
            context->allocator.free(&context->allocator, t);
            return PHX_FALSE;
        }
    }
    else
    {
        t->relation = PHX_BST_GENERAL;

        const PHX_BlockByteSize totalBytes = (PHX_BlockByteSize)device->blockCount * sourceBlockSize;
        outBlockCount = (PHX_BlockSize)(totalBytes / targetBlockSize);

        const PHX_BlockSize chunkBytes = PHX_BST_CHUNK_TARGET_BLOCKS * targetBlockSize;
        const PHX_BlockSize maxSrcBlocks = PHX_CeilDiv(chunkBytes, sourceBlockSize) + 1;
        t->scratchSourceBlocks = maxSrcBlocks;
        t->scratch = (PHX_Byte*)context->allocator.allocate(&context->allocator, maxSrcBlocks * sourceBlockSize);
        if (!t->scratch)
        {
            context->allocator.free(&context->allocator, t);
            return PHX_FALSE;
        }
    }

    outDevice->blockSize = targetBlockSize;
    outDevice->blockCount = outBlockCount;
    outDevice->data = t;

    outDevice->read = BST_Read;
    outDevice->write = BST_Write;
    outDevice->close = BST_Close;

    outDevice->type = PHX_BLOCK_SIZE_TRANSFORM_TYPE;

    outDevice->readonly = device->readonly;

    return PHX_TRUE;
}
