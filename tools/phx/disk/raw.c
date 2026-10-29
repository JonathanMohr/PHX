#include "raw.h"
#include "phx/types.h"

#include <phx/base/intrinsic.h>
#include <phx/utility/file.h>
#include <phx/utility/device.h>

#include <zero.h>

#define RAW_Type "RAW-DISK"

static const PHX_SectorSize sectorSize = 512;

typedef struct PHX_RAW_Data
{
    PHX_Context* context;
    PHX_Disk_Device fileDevice;
    PHX_Disk_Device transformedDevice;
} PHX_RAW_Data;


static PHX_SectorSize PHX_RAW_Read(PHX_Disk_Device* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_RAW_Data* data = device->data;
    PHX_SectorSize c = data->transformedDevice.read(&data->transformedDevice, buffer, block, count);
    return c;
}

static PHX_SectorSize PHX_RAW_Write(PHX_Disk_Device* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_RAW_Data* data = device->data;
    return data->transformedDevice.write(&data->transformedDevice, buffer, block, count);
}

static void PHX_RAW_Close(PHX_Disk_Device* device)
{
    PHX_RAW_Data* data = device->data;

    data->transformedDevice.close(&data->transformedDevice);

    PHX_Context* context = data->context;
    context->allocator->free(context->allocator, data);
}


static PHX_Bool PHX_RAW_GetDevice(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    PHX_RAW_Data* data = context->allocator->allocate(context->allocator, sizeof(PHX_RAW_Data));
    if (!data)
        return PHX_FALSE;

    data->context = context;

    PHX_File_GetDiskDevice(device, &data->fileDevice);

    if (PHX_BlockCountTransformDevice(context, &data->fileDevice, sectorSize, PHX_TRUE, &data->transformedDevice) != PHX_TRUE)
    {
        context->allocator->free(context->allocator, data);
        return PHX_FALSE;
    }

    out->sectorSize = sectorSize;
    out->totalSectors = data->transformedDevice.totalSectors;
    out->data = data;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Read;
    out->write = PHX_RAW_Write;

    out->close = PHX_RAW_Close;

    out->type = RAW_Type;

    out->totalCylinders = out->totalSectors / (255 * 63);
    out->totalHeads = 255;
    out->sectorsPerTrack = 63;

    out->readonly = readonly;

    return PHX_TRUE;
}

static PHX_Bool PHX_RAW_FormatDevice(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out)
{
    if (!readonly && device->readonly)
        return PHX_FALSE;

    PHX_RAW_Data* data = context->allocator->allocate(context->allocator, sizeof(PHX_RAW_Data));
    if (!data)
        return PHX_FALSE;

    data->context = context;

    PHX_File_GetDiskDevice(device, &data->fileDevice);

    if (PHX_BlockCountTransformDevice(context, &data->fileDevice, sectorSize, PHX_TRUE, &data->transformedDevice) != PHX_TRUE)
    {
        context->allocator->free(context->allocator, data);
        return PHX_FALSE;
    }

    out->sectorSize = sectorSize;
    out->totalSectors = data->transformedDevice.totalSectors;
    out->data = data;

    out->sectorOffset = 0;

    out->read = PHX_RAW_Read;
    out->write = PHX_RAW_Write;

    out->close = PHX_RAW_Close;

    out->type = RAW_Type;

    out->totalCylinders = out->totalSectors / (255 * 63);
    out->totalHeads = 255;
    out->sectorsPerTrack = 63;

    out->readonly = readonly;

    /* TODO: Implement again
    if (!context->fast)
    {
        const PHX_Byte* buffer = zeroBuffer;
        PHX_Byte* tmpBuffer = PHX_NULL;
        PHX_Size bufferSize = sizeof(zeroBuffer);

        if (device->fileSize > bufferSize)
        {
            tmpBuffer = context->allocator->allocate(context->allocator, device->fileSize);
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
            if (device->ops->write(device, buffer, i, blocksToWrite) != blocksToWrite)
            {
                if (tmpBuffer) context->allocator->free(context->allocator, tmpBuffer);
                return PHX_FALSE;
            }
        }

        if (tmpBuffer) context->allocator->free(context->allocator, tmpBuffer);
    }
    */

    return PHX_TRUE;
}

PHX_Disk_Interface PHX_RAW_Interface = {
    PHX_RAW_GetDevice,
    PHX_RAW_FormatDevice,
    "RAW-DISK-INTERFACE",
    "raw"
};
