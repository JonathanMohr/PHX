#ifndef PHX_DEVICE_DEVICE_H
#define PHX_DEVICE_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <file.h>
#include <context.h>

/*
    List of dynamic container formats:
        VDI
        VMDK
        VHD(X)
        QCOW2

    List of other static container formats:
        E01
*/


typedef struct PHX_DiskDevice
{
    // PHX_BlockSize sectorSize;
    // PHX_BlockSize totalSectors;

    PHX_BlockSize blockSize;
    PHX_BlockSize blockCount;

    PHX_BlockSize sectorOffset;
    void* data;

    PHX_BlockSize (*read)(struct PHX_DiskDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count);
    PHX_BlockSize (*write)(struct PHX_DiskDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count);

    void (*close)(struct PHX_DiskDevice* device);

    const char* type;

    // PHX_u16 totalCylinders;
    // PHX_u8 totalHeads;
    // PHX_u8 sectorsPerTrack;

    PHX_Bool readonly;
} PHX_DiskDevice;

#define PHX_BlockDevice PHX_DiskDevice


typedef struct PHX_Disk_Interface
{
    PHX_Bool (*getDevice)(PHX_Context* context, PHX_FileDevice* device, PHX_Bool readonly, PHX_DiskDevice* out);
    PHX_Bool (*formatDevice)(PHX_Context* context, PHX_FileDevice* device, PHX_Bool readonly, PHX_DiskDevice* out);

    const char* name;
    const char* type;
} PHX_Disk_Interface;


extern PHX_Disk_Interface* PHX_Disk_Interfaces[];
extern PHX_Size PHX_Disk_InterfaceCount;

PHX_Bool PHX_BlockCountTransformDevice(PHX_Context* context, PHX_DiskDevice* device, PHX_BlockSize targetBlockSize, PHX_Bool takeOwnership, PHX_DiskDevice* outDevice);


#ifdef __cplusplus
}
#endif

#endif
