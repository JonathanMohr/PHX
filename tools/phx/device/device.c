#include "device.h"

#include "raw.h"
#include "vdi.h"

PHX_Disk_Interface* PHX_Disk_Interfaces[] = {
    &PHX_RAW_Interface,
    &PHX_VDI_Interface
};
PHX_Size PHX_Disk_InterfaceCount = sizeof(PHX_Disk_Interfaces) / sizeof(PHX_Disk_Interfaces[0]);
