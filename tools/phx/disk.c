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
