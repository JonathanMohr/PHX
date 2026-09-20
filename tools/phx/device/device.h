#ifndef PHX_DEVICE_DEVICE_H
#define PHX_DEVICE_DEVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>

/*
    List of dynamic container formats:
        VDI
        VMDK
        VHD(X)
        QCOW2

    List of other static container formats:
        E01
*/


typedef struct PHX_BlockDevice
{
    PHX_BlockSize blockSize;
    PHX_BlockSize blockCount;
    void* data;

    PHX_BlockSize (*read)(struct PHX_BlockDevice* device, void* buffer, PHX_BlockSize block, PHX_BlockSize count);
    PHX_BlockSize (*write)(struct PHX_BlockDevice* device, const void* buffer, PHX_BlockSize block, PHX_BlockSize count);

    void (*close)(struct PHX_BlockDevice* device);

    const char* type;

    PHX_Bool readonly;
} PHX_BlockDevice;


typedef struct PHX_Disk_Interface
{
    PHX_Bool (*getDevice)(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out);
    PHX_Bool (*formatDevice)(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out);

    const char* type;
    const char* name;
} PHX_Disk_Interface;


extern PHX_Disk_Interface* PHX_Disk_Interfaces[];
extern PHX_Size PHX_Disk_InterfaceCount;

PHX_Bool PHX_BlockCountTransformDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_BlockSize targetBlockSize, PHX_Bool takeOwnership, PHX_BlockDevice* outDevice);


#ifdef __cplusplus
}
#endif

#endif
