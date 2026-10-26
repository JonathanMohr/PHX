#ifndef PHX_DISK_DISK_H
#define PHX_DISK_DISK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/context.h>
#include <phx/interface/disk.h>

/*
    List of dynamic container formats:
        VDI
        VMDK
        VHD(X)
        QCOW2

    List of other static container formats:
        E01
*/

extern PHX_Disk_Interface* PHX_Disk_Interfaces[];
extern PHX_Size PHX_Disk_InterfaceCount;


#ifdef __cplusplus
}
#endif

#endif
