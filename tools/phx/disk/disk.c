#include "disk.h"

#include "disk/raw.h"
#include "disk/vdi.h"
#include "disk/vhd.h"

PHX_Disk_Interface* PHX_Disk_Interfaces[] = {
    // &PHX_VDI_Interface,
    // &PHX_VHD_Interface,
    &PHX_RAW_Interface
};
PHX_Size PHX_Disk_InterfaceCount = sizeof(PHX_Disk_Interfaces) / sizeof(PHX_Disk_Interfaces[0]);
