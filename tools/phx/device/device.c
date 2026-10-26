#include "device.h"

#include <base.h>

#include "raw.h"
#include "types.h"
#include "vdi.h"
#include "vhd.h"

PHX_Disk_Interface* PHX_Disk_Interfaces[] = {
    // &PHX_VDI_Interface,
    // &PHX_VHD_Interface,
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
} PHX_SectorSizeRelation;

#define PHX_BST_CHUNK_TARGET_BLOCKS 128

typedef struct PHX_SectorSizeTransform
{
    PHX_Context* context;
    PHX_BlockDevice* source;

    PHX_Byte* scratch;
    PHX_SectorSize scratchSourceBlocks;

    PHX_SectorSize targetBlockSize;
    PHX_SectorSize ratio;
    PHX_SectorSizeRelation relation;

    PHX_Bool ownsSource;
} PHX_SectorSizeTransform;

static inline PHX_BlockByteSize PHX_CeilDiv(PHX_BlockByteSize a, PHX_BlockByteSize b) { return (a + b - 1) / b; }


static PHX_SectorSize BST_Read_Identity(PHX_SectorSizeTransform* t, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    return t->source->read(t->source, buffer, block, count);
}

static PHX_SectorSize BST_Write_Identity(PHX_SectorSizeTransform* t, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    return t->source->write(t->source, buffer, block, count);
}


static PHX_SectorSize BST_Read_TargetMultiple(PHX_SectorSizeTransform* t, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize srcBlock = block * t->ratio;
    const PHX_SectorSize srcCount = count * t->ratio;
    const PHX_SectorSize srcRead = t->source->read(t->source, buffer, srcBlock, srcCount);
    return srcRead / t->ratio;
}

static PHX_SectorSize BST_Write_TargetMultiple(PHX_SectorSizeTransform* t, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize srcBlock = block * t->ratio;
    const PHX_SectorSize srcCount = count * t->ratio;
    const PHX_SectorSize srcWritten = t->source->write(t->source, buffer, srcBlock, srcCount);
    return srcWritten / t->ratio;
}


static PHX_SectorSize BST_Read_SourceMultiple(PHX_SectorSizeTransform* t, PHX_Byte* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize ratio = t->ratio;
    const PHX_SectorSize targetBlockSize = t->targetBlockSize;
    PHX_SectorSize done = 0;

    while (done < count)
    {
        const PHX_SectorSize remaining = count - done;
        const PHX_SectorSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_SectorSize targetBlock = block + done;

        const PHX_SectorSize firstSrcBlock = targetBlock / ratio;
        const PHX_SectorSize lastSrcBlock  = (targetBlock + chunk - 1) / ratio;
        const PHX_SectorSize srcBlockCount = lastSrcBlock - firstSrcBlock + 1;
        const PHX_SectorSize subOffset     = targetBlock % ratio;

        const PHX_SectorSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead == 0)
            return done;

        const PHX_SectorSize availableTargetBlocks = (srcRead * ratio) - subOffset;
        const PHX_SectorSize copyBlocks = (availableTargetBlocks < chunk) ? availableTargetBlocks : chunk;

        memcpy(buffer + done * targetBlockSize, t->scratch + subOffset * targetBlockSize, copyBlocks * targetBlockSize);

        done += copyBlocks;
        if (copyBlocks < chunk)
            break;
    }

    return done;
}

static PHX_SectorSize BST_Write_SourceMultiple(PHX_SectorSizeTransform* t, const PHX_Byte* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize ratio = t->ratio;
    const PHX_SectorSize targetBlockSize = t->targetBlockSize;
    PHX_SectorSize done = 0;

    while (done < count)
    {
        const PHX_SectorSize remaining = count - done;
        const PHX_SectorSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_SectorSize targetBlock = block + done;

        if (targetBlock % ratio == 0 && chunk % ratio == 0)
        {
            const PHX_SectorSize srcBlock = targetBlock / ratio;
            const PHX_SectorSize srcCount = chunk / ratio;
            const PHX_SectorSize srcWritten = t->source->write(t->source, buffer + done * targetBlockSize, srcBlock, srcCount);
            const PHX_SectorSize writtenTargetBlocks = srcWritten * ratio;
            done += writtenTargetBlocks;
            if (writtenTargetBlocks < chunk)
                break;
            continue;
        }

        const PHX_SectorSize firstSrcBlock = targetBlock / ratio;
        const PHX_SectorSize lastSrcBlock  = (targetBlock + chunk - 1) / ratio;
        const PHX_SectorSize srcBlockCount = lastSrcBlock - firstSrcBlock + 1;
        const PHX_SectorSize subOffset     = targetBlock % ratio;

        const PHX_SectorSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead != srcBlockCount)
            return done;

        memcpy(t->scratch + subOffset * targetBlockSize, buffer + done * targetBlockSize, chunk * targetBlockSize);

        const PHX_SectorSize srcWritten = t->source->write(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcWritten != srcBlockCount)
            return done;

        done += chunk;
    }

    return done;
}


static PHX_SectorSize BST_Read_General(PHX_SectorSizeTransform* t, PHX_Byte* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize targetBlockSize = t->targetBlockSize;
    const PHX_SectorSize sourceBlockSize = t->source->sectorSize;
    PHX_SectorSize done = 0;

    while (done < count)
    {
        const PHX_SectorSize remaining = count - done;
        const PHX_SectorSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_SectorSize targetBlock = block + done;

        const PHX_BlockByteSize byteOffset = (PHX_BlockByteSize)targetBlock * targetBlockSize;
        const PHX_BlockByteSize byteCount  = (PHX_BlockByteSize)chunk * targetBlockSize;
        const PHX_BlockByteSize byteEnd    = byteOffset + byteCount;

        const PHX_SectorSize firstSrcBlock = (PHX_SectorSize)(byteOffset / sourceBlockSize);
        const PHX_SectorSize lastSrcBlock  = (PHX_SectorSize)PHX_CeilDiv(byteEnd, sourceBlockSize);
        const PHX_SectorSize srcBlockCount = lastSrcBlock - firstSrcBlock;
        const PHX_BlockByteSize relOffset = byteOffset - (PHX_BlockByteSize)firstSrcBlock * sourceBlockSize;

        const PHX_SectorSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        const PHX_BlockByteSize bytesRead = (PHX_BlockByteSize)srcRead * sourceBlockSize;

        const PHX_BlockByteSize usable = (bytesRead > relOffset) ? (bytesRead - relOffset) : 0;
        const PHX_BlockByteSize bytesToCopy = (usable < byteCount) ? usable : byteCount;

        if (bytesToCopy > 0)
            memcpy(buffer + done * targetBlockSize, t->scratch + relOffset, bytesToCopy);

        const PHX_SectorSize copyBlocks = bytesToCopy / targetBlockSize;
        done += copyBlocks;
        if (copyBlocks < chunk)
            break;
    }

    return done;
}

static PHX_SectorSize BST_Write_General(PHX_SectorSizeTransform* t, const PHX_Byte* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    const PHX_SectorSize targetBlockSize = t->targetBlockSize;
    const PHX_SectorSize sourceBlockSize = t->source->sectorSize;
    PHX_SectorSize done = 0;

    while (done < count)
    {
        const PHX_SectorSize remaining = count - done;
        const PHX_SectorSize chunk = (remaining < PHX_BST_CHUNK_TARGET_BLOCKS) ? remaining : PHX_BST_CHUNK_TARGET_BLOCKS;
        const PHX_SectorSize targetBlock = block + done;

        const PHX_BlockByteSize byteOffset = (PHX_BlockByteSize)targetBlock * targetBlockSize;
        const PHX_BlockByteSize byteCount  = (PHX_BlockByteSize)chunk * targetBlockSize;
        const PHX_BlockByteSize byteEnd    = byteOffset + byteCount;

        const PHX_SectorSize firstSrcBlock = (PHX_SectorSize)(byteOffset / sourceBlockSize);
        const PHX_SectorSize lastSrcBlock  = (PHX_SectorSize)PHX_CeilDiv(byteEnd, sourceBlockSize);
        const PHX_SectorSize srcBlockCount = lastSrcBlock - firstSrcBlock;
        const PHX_BlockByteSize relOffset = byteOffset - (PHX_BlockByteSize)firstSrcBlock * sourceBlockSize;

        if (relOffset == 0 && byteCount % sourceBlockSize == 0)
        {
            const PHX_SectorSize srcWritten = t->source->write(t->source, buffer + done * targetBlockSize, firstSrcBlock, srcBlockCount);
            const PHX_BlockByteSize writtenBytes = (PHX_BlockByteSize)srcWritten * sourceBlockSize;
            const PHX_SectorSize writtenBlocks = (PHX_SectorSize)(writtenBytes / targetBlockSize);
            done += writtenBlocks;
            if (writtenBlocks < chunk)
                break;
            continue;
        }

        const PHX_SectorSize srcRead = t->source->read(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcRead != srcBlockCount)
            return done;

        memcpy(t->scratch + relOffset, buffer + done * targetBlockSize, byteCount);

        const PHX_SectorSize srcWritten = t->source->write(t->source, t->scratch, firstSrcBlock, srcBlockCount);
        if (srcWritten != srcBlockCount)
            return done;

        done += chunk;
    }

    return done;
}


static PHX_SectorSize BST_Read(PHX_BlockDevice* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_SectorSizeTransform* t = (PHX_SectorSizeTransform*)device->data;
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

static PHX_SectorSize BST_Write(PHX_BlockDevice* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_SectorSizeTransform* t = (PHX_SectorSizeTransform*)device->data;
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
    PHX_SectorSizeTransform* t = (PHX_SectorSizeTransform*)device->data;
    PHX_Context* context = t->context;
    if (t->ownsSource && t->source->close)
        t->source->close(t->source);
    if (t->scratch)
        context->allocator.free(&context->allocator, t->scratch);
    context->allocator.free(&context->allocator, t);
}


PHX_Bool PHX_BlockCountTransformDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_SectorSize targetBlockSize, PHX_Bool takeOwnership, PHX_BlockDevice* outDevice)
{
    if (targetBlockSize == 0 || device->sectorSize == 0)
        return PHX_FALSE;

    PHX_SectorSizeTransform* t = (PHX_SectorSizeTransform*)context->allocator.allocate(&context->allocator, sizeof(PHX_SectorSizeTransform));
    if (!t)
        return PHX_FALSE;

    memset(t, 0, sizeof(PHX_SectorSizeTransform));
    t->context = context;
    t->source = device;
    t->ownsSource = takeOwnership;
    t->targetBlockSize = targetBlockSize;

    const PHX_SectorSize sourceBlockSize = device->sectorSize;
    PHX_SectorSize outBlockCount;

    if (targetBlockSize == sourceBlockSize)
    {
        t->relation = PHX_BST_IDENTITY;
        outBlockCount = device->totalSectors;
    }
    else if (targetBlockSize % sourceBlockSize == 0)
    {
        t->relation = PHX_BST_TARGET_MULTIPLE;
        t->ratio = targetBlockSize / sourceBlockSize;
        outBlockCount = device->totalSectors / t->ratio;
    }
    else if (sourceBlockSize % targetBlockSize == 0)
    {
        t->relation = PHX_BST_SOURCE_MULTIPLE;
        t->ratio = sourceBlockSize / targetBlockSize;
        outBlockCount = device->totalSectors * t->ratio;

        const PHX_SectorSize maxSrcBlocks = PHX_CeilDiv(PHX_BST_CHUNK_TARGET_BLOCKS, t->ratio) + 1;
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

        const PHX_BlockByteSize totalBytes = (PHX_BlockByteSize)device->totalSectors * sourceBlockSize;
        outBlockCount = (PHX_SectorSize)(totalBytes / targetBlockSize);

        const PHX_SectorSize chunkBytes = PHX_BST_CHUNK_TARGET_BLOCKS * targetBlockSize;
        const PHX_SectorSize maxSrcBlocks = PHX_CeilDiv(chunkBytes, sourceBlockSize) + 1;
        t->scratchSourceBlocks = maxSrcBlocks;
        t->scratch = (PHX_Byte*)context->allocator.allocate(&context->allocator, maxSrcBlocks * sourceBlockSize);
        if (!t->scratch)
        {
            context->allocator.free(&context->allocator, t);
            return PHX_FALSE;
        }
    }

    outDevice->sectorSize = targetBlockSize;
    outDevice->totalSectors = outBlockCount;
    outDevice->data = t;

    outDevice->sectorOffset = device->sectorOffset;

    outDevice->read = BST_Read;
    outDevice->write = BST_Write;
    outDevice->close = BST_Close;

    outDevice->type = PHX_BLOCK_SIZE_TRANSFORM_TYPE;

    outDevice->totalCylinders = device->totalCylinders;
    outDevice->totalHeads = device->totalHeads;
    outDevice->sectorsPerTrack = device->sectorsPerTrack;

    outDevice->readonly = device->readonly;

    return PHX_TRUE;
}
