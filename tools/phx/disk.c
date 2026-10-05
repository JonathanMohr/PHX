#include "disk.h"

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

PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_BlockDevice* fileDevice, PHX_BlockDevice* diskDeviceOut)
{
    for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
    {
        PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
        if (interface->getDevice(context, fileDevice, PHX_FALSE, diskDeviceOut) == PHX_TRUE)
            return PHX_SUCCESS;
    }

    return PHX_ERROR_FORMAT;
}
