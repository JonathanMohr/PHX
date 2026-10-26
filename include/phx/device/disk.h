#ifndef PHX_DEVICE_DISK_H
#define PHX_DEVICE_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>

typedef struct PHX_Disk_Device
{
    PHX_SectorSize sectorSize;
    PHX_SectorSize totalSectors;

    PHX_SectorSize sectorOffset;
    void* data;

    PHX_SectorSize (*read)(struct PHX_Disk_Device* device, void* buffer, PHX_SectorSize block, PHX_SectorSize count);
    PHX_SectorSize (*write)(struct PHX_Disk_Device* device, const void* buffer, PHX_SectorSize block, PHX_SectorSize count);

    void (*close)(struct PHX_Disk_Device* device);

    const char* type;

    PHX_SectorSize totalCylinders;
    PHX_u8 totalHeads;
    PHX_u8 sectorsPerTrack;

    PHX_Bool readonly;
} PHX_Disk_Device;

#ifdef __cplusplus
}
#endif

#endif
