#include "raw.h"

#include <base.h>
#include <zero.h>
#include <disk.h>

#define RAW_Type "RAW-DISK"

static const PHX_BlockSize blockSize = 512;

typedef struct PHX_RAW_Data
{
    PHX_Context* context;
    PHX_BlockDevice fileDevice;
    PHX_BlockDevice transformedDevice;
} PHX_RAW_Data;


static PHX_BlockSize PHX_RAW_Read(PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_RAW_Data* data = device->data;
    return data->transformedDevice.read(&data->transformedDevice, buffer, block, count);
}

static PHX_BlockSize PHX_RAW_Write(PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count)
{
    PHX_RAW_Data* data = device->data;
    return data->transformedDevice.write(&data->transformedDevice, buffer, block, count);
}

static void PHX_RAW_Close(PHX_BlockDevice* device)
{
    PHX_RAW_Data* data = device->data;

    data->transformedDevice.close(&data->transformedDevice);

    PHX_Context* context = data->context;
    context->allocator.free(&context->allocator, data);
}


static PHX_Bool PHX_RAW_GetDevice(PHX_Context* context, PHX_FileDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    PHX_RAW_Data* data = context->allocator.allocate(&context->allocator, sizeof(PHX_RAW_Data));
    if (!data)
        return PHX_FALSE;

    data->context = context;

    PHX_File_GetDiskDevice(device, &data->fileDevice);

    if (PHX_BlockCountTransformDevice(context, &data->fileDevice, blockSize, PHX_TRUE, &data->transformedDevice) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_FALSE;
    }

    out->blockSize = blockSize;
    out->blockCount = data->transformedDevice.blockCount;
    out->data = data;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Read;
    out->write = PHX_RAW_Write;

    out->close = PHX_RAW_Close;

    out->type = RAW_Type;

    out->readonly = readonly;

    return PHX_TRUE;
}

static PHX_Bool PHX_RAW_FormatDevice(PHX_Context* context, PHX_FileDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    PHX_RAW_Data* data = context->allocator.allocate(&context->allocator, sizeof(PHX_RAW_Data));
    if (!data)
        return PHX_FALSE;

    data->context = context;

    PHX_File_GetDiskDevice(device, &data->fileDevice);

    if (PHX_BlockCountTransformDevice(context, &data->fileDevice, blockSize, PHX_TRUE, &data->transformedDevice) != PHX_TRUE)
    {
        context->allocator.free(&context->allocator, data);
        return PHX_FALSE;
    }

    out->blockSize = blockSize;
    out->blockCount = data->transformedDevice.blockCount;
    out->data = data;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Read;
    out->write = PHX_RAW_Write;

    out->close = PHX_RAW_Close;

    out->type = RAW_Type;

    out->readonly = readonly;

    if (!context->fast)
    {
        const PHX_Byte* buffer = zeroBuffer;
        PHX_Byte* tmpBuffer = PHX_NULL;
        PHX_Size bufferSize = sizeof(zeroBuffer);

        if (device->fileSize > bufferSize)
        {
            tmpBuffer = context->allocator.allocate(&context->allocator, device->fileSize);
            if (!tmpBuffer)
                return PHX_FALSE;

            memset(tmpBuffer, 0, device->fileSize);

            buffer = tmpBuffer;
            bufferSize = device->fileSize;
        }

        const PHX_FileSize blocksPerCycle = bufferSize / device->fileSize;
        for (PHX_FileSize i = 0; i < device->fileSize; i += blocksPerCycle)
        {
            const PHX_FileSize blocksToWrite = (i + blocksPerCycle > device->fileSize) ? (device->fileSize - i) : blocksPerCycle;
            if (PHX_File_Write(device, buffer, i, blocksToWrite) != blocksToWrite)
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
    "RAW-DISK-INTERFACE",
    "raw"
};
