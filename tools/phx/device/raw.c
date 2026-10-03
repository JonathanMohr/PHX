#include "raw.h"
#include "device.h"

#include <base.h>
#include <zero.h>

#define RAW_Type "RAW-DISK"

static const PHX_BlockSize blockSize = 512;

static PHX_Bool PHX_RAW_GetDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    if (PHX_BlockCountTransformDevice(context, device, blockSize, PHX_TRUE, out) != PHX_TRUE)
        return PHX_FALSE;

    // TODO: Type

    out->readonly = readonly;

    return PHX_TRUE;
}

static PHX_Bool PHX_RAW_FormatDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    if (PHX_BlockCountTransformDevice(context, device, blockSize, PHX_TRUE, out) != PHX_TRUE)
        return PHX_FALSE;

    out->readonly = readonly;

    if (!context->fast)
    {
        const PHX_Byte* buffer = zeroBuffer;
        PHX_Byte* tmpBuffer = PHX_NULL;
        PHX_Size bufferSize = sizeof(zeroBuffer);

        if (device->blockSize > bufferSize)
        {
            tmpBuffer = context->allocator.allocate(&context->allocator, device->blockSize);
            if (!tmpBuffer)
                return PHX_FALSE;

            memset(tmpBuffer, 0, device->blockSize);

            buffer = tmpBuffer;
            bufferSize = device->blockSize;
        }

        const PHX_BlockSize blocksPerCycle = bufferSize / device->blockSize;
        for (PHX_BlockSize i = 0; i < device->blockCount; i += blocksPerCycle)
        {
            const PHX_BlockSize blocksToWrite = (i + blocksPerCycle > device->blockCount) ? (device->blockCount - i) : blocksPerCycle;
            if (device->write(device, buffer, i, blocksToWrite) != blocksToWrite)
            {
                if (tmpBuffer) context->allocator.free(&context->allocator, tmpBuffer);
                return PHX_FALSE;
            }
        }

        if (tmpBuffer) context->allocator.free(&context->allocator, tmpBuffer);
    }

    return PHX_TRUE;
}

PHX_Disk_Interface PHX_RAW_Interface = {
    PHX_RAW_GetDevice,
    PHX_RAW_FormatDevice,
    RAW_Type,
    "RAW-DISK-INTERFACE"
};
