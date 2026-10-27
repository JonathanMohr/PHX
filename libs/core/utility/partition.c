#include <phx/utility/partition.h>

typedef struct PHX_Partition_Device
{
    PHX_Context* context;
    PHX_Disk_Device* device;
    PHX_SectorSize start;
    PHX_SectorSize size;
} PHX_Partition_Device;

static PHX_SectorSize PHX_Partition_Read(PHX_Disk_Device* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_Partition_Device* data = (PHX_Partition_Device*)device->data;

    if (block > data->size)
        return 0;

    if (count > data->size - block)
        count = data->size - block;

    return data->device->read(data->device, buffer, block + data->start, count);
}

static PHX_SectorSize PHX_Partition_Write(PHX_Disk_Device* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_Partition_Device* data = (PHX_Partition_Device*)device->data;

    if (block > data->size)
        return 0;

    if (count > data->size - block)
        count = data->size - block;

    return data->device->write(data->device, buffer, block + data->start, count);
}

static void PHX_Partition_Close(PHX_Disk_Device* device)
{
    PHX_Partition_Device* data = (PHX_Partition_Device*)device->data;
    PHX_Context* context = data->context;
    context->allocator->free(context->allocator, data);
}

PHX_Bool PHX_Partition_CreateDevice(PHX_Context* context, PHX_Disk_Device* device, const PHX_Partition* partition, PHX_Disk_Device* outDevice)
{
    if (partition->start > device->totalSectors || partition->size > device->totalSectors - partition->start)
        return PHX_FALSE;

    PHX_Partition_Device* data = (PHX_Partition_Device*)context->allocator->allocate(context->allocator, sizeof(PHX_Partition_Device));
    if (!data)
        return PHX_FALSE;

    data->context = context;
    data->device = device;
    data->start = partition->start;
    data->size = partition->size;

    outDevice->sectorSize = device->sectorSize;
    outDevice->totalSectors = partition->size;
    outDevice->data = data;

    outDevice->sectorOffset = partition->start;

    outDevice->read = PHX_Partition_Read;
    outDevice->write = PHX_Partition_Write;
    outDevice->close = PHX_Partition_Close;

    outDevice->type = "PARTITION-DEVICE";

    outDevice->totalCylinders = device->totalCylinders;
    outDevice->totalHeads = device->totalHeads;
    outDevice->sectorsPerTrack = device->sectorsPerTrack;

    outDevice->readonly = device->readonly;

    return PHX_TRUE;
}
