#include "disk.h"
#include "filesystem/filesystem.h"

#include <string.h>

PHX_Disk_Interface* PHX_Disk_FindInterfaceByType(const char* type)
{
    for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
    {
        PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];

        if (strcmp(interface->type, type) == 0)
            return interface;
    }

    return PHX_NULL;
}

PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_FileDevice* fileDevice, PHX_BlockDevice* diskDeviceOut)
{
    for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
    {
        PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
        if (interface->getDevice(context, fileDevice, PHX_FALSE, diskDeviceOut) == PHX_TRUE)
            return PHX_SUCCESS;
    }

    return PHX_ERROR_FORMAT;
}


PHX_Partition_Interface* PHX_Partition_FindInterfaceByType(const char* type)
{
    for (PHX_Size i = 0; i < PHX_Partition_InterfaceCount; i++)
    {
        PHX_Partition_Interface* interface = PHX_Partition_Interfaces[i];

        if (strcmp(interface->type, type) == 0)
            return interface;
    }

    return PHX_NULL;
}

PHX_Result PHX_Partition_Open(PHX_Context* context, PHX_BlockDevice* diskDevice, PHX_Partition_Interface** interfaceOut, PHX_Partition_Table* partitionTableOut)
{
    for (PHX_Size i = 0; i < PHX_Partition_InterfaceCount; i++)
    {
        PHX_Partition_Interface* interface = PHX_Partition_Interfaces[i];
        if (interface->readTable(context, diskDevice, partitionTableOut) == PHX_TRUE)
        {
            *interfaceOut = interface;
            return PHX_SUCCESS;
        }
    }

    return PHX_ERROR_FORMAT;
}


PHX_Filesystem_Interface* PHX_Filesystem_FindInterfaceByType(const char* type)
{
    for (PHX_Size i = 0; i < PHX_Filesystem_InterfaceCount; i++)
    {
        PHX_Filesystem_Interface* interface = PHX_Filesystem_Interfaces[i];

        if (strcmp(interface->type, type) == 0)
            return interface;
    }

    return PHX_NULL;
}

PHX_Result PHX_Filesystem_Open(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* filesystemOut)
{
    for (PHX_Size i = 0; i < PHX_Filesystem_InterfaceCount; i++)
    {
        PHX_Filesystem_Interface* interface = PHX_Filesystem_Interfaces[i];

        PHX_Result result = interface->openFilesystem(context, device, filesystemOut, PHX_FALSE);
        if (result == PHX_SUCCESS)
            return PHX_SUCCESS;

        if (result == PHX_ERROR_FORMAT)
            continue;

        return result;
    }

    return PHX_ERROR_FORMAT;
}


static PHX_SectorSize PHX_DiskDevice_File_Read(PHX_BlockDevice* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    return PHX_File_Read(device->data, buffer, block, count);
}

static PHX_SectorSize PHX_DiskDevice_File_Write(PHX_BlockDevice* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    return PHX_File_Write(device->data, buffer, block, count);
}

static void PHX_DiskDevice_File_Close(PHX_BlockDevice* device)
{
    PHX_File_Close(device->data);
}

void PHX_File_GetDiskDevice(PHX_FileDevice* fileDevice, PHX_DiskDevice* out)
{
    out->sectorSize = 1;
    out->totalSectors = fileDevice->fileSize;
    out->data = fileDevice;

    out->sectorOffset = 0;

    out->read = PHX_DiskDevice_File_Read;
    out->write = PHX_DiskDevice_File_Write;

    out->close = PHX_DiskDevice_File_Close;

    out->type = "FILE-DEVICE";

    out->totalCylinders = out->totalSectors;
    out->totalHeads = 1;
    out->sectorsPerTrack = 1;

    out->readonly = fileDevice->readonly;
}
