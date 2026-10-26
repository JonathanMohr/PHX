#include <phx/utility/file.h>

static PHX_SectorSize PHX_Disk_Device_File_Read(PHX_Disk_Device* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_File_Device* fileDevice = device->data;
    return fileDevice->ops->read(fileDevice, buffer, block, count);
}

static PHX_SectorSize PHX_Disk_Device_File_Write(PHX_Disk_Device* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count)
{
    PHX_File_Device* fileDevice = device->data;
    return fileDevice->ops->write(fileDevice, buffer, block, count);
}

static void PHX_Disk_Device_File_Close(PHX_Disk_Device* device)
{
    PHX_File_Device* fileDevice = device->data;
    fileDevice->ops->close(fileDevice);
}

void PHX_File_GetDiskDevice(PHX_File_Device* fileDevice, PHX_Disk_Device* out)
{
    out->sectorSize = 1;
    out->totalSectors = fileDevice->fileSize;
    out->data = fileDevice;

    out->sectorOffset = 0;

    out->read = PHX_Disk_Device_File_Read;
    out->write = PHX_Disk_Device_File_Write;

    out->close = PHX_Disk_Device_File_Close;

    out->type = "FILE-DEVICE";

    out->totalCylinders = out->totalSectors;
    out->totalHeads = 1;
    out->sectorsPerTrack = 1;

    out->readonly = fileDevice->readonly;
}
