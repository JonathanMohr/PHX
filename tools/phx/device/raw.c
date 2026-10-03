#include "raw.h"

#include <base.h>
#include <zero.h>

#define RAW_Type "RAW-DISK"

static PHX_BlockSize PHX_RAW_Device_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_BlockDevice* parent = (PHX_BlockDevice*)device->data;
    return parent->read(parent, buffer, block, count);
}

static PHX_BlockSize PHX_RAW_Device_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_BlockDevice* parent = (PHX_BlockDevice*)device->data;
    return parent->write(parent, buffer, block, count);
}

static void PHX_RAW_Device_Close(PHX_BlockDevice* device)
{
    PHX_BlockDevice* parent = (PHX_BlockDevice*)device->data;
    parent->close(parent);
}

static PHX_Bool PHX_RAW_GetDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    (void)context;

    if (!readonly && device->readonly)
        return PHX_FALSE;

    out->blockSize = device->blockSize;
    out->blockCount = device->blockCount;
    out->data = (void*)device;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Device_Read;
    out->write = PHX_RAW_Device_Write;
    out->close = device->close;

    out->type = RAW_Type;

    out->readonly = readonly;

    return PHX_TRUE;
}

static PHX_Bool PHX_RAW_FormatDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    out->blockSize = device->blockSize;
    out->blockCount = device->blockCount;
    out->data = device;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Device_Read;
    out->write = PHX_RAW_Device_Write;
    out->close = PHX_RAW_Device_Close;

    out->type = RAW_Type;

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
