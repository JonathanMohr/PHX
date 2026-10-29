#include "interfaces.h"

#include "disk/disk.h"
#include "partition/partition.h"
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

PHX_Result PHX_Disk_Open(PHX_Context* context, PHX_File_Device* fileDevice, PHX_Disk_Device* diskDeviceOut, PHX_Bool readonly)
{
    for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
    {
        PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
        if (interface->getDevice(context, fileDevice, readonly, diskDeviceOut) == PHX_TRUE)
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

PHX_Result PHX_Partition_Open(PHX_Context* context, PHX_Disk_Device* diskDevice, PHX_Partition_Table_Device* partitionTableOut)
{
    for (PHX_Size i = 0; i < PHX_Partition_InterfaceCount; i++)
    {
        PHX_Partition_Interface* interface = PHX_Partition_Interfaces[i];

        PHX_Result result = interface->openPartitionTable(context, diskDevice, partitionTableOut);
        if (result == PHX_SUCCESS)
            return PHX_SUCCESS;

        if (result == PHX_ERROR_FORMAT)
            continue;

        return result;
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

PHX_Result PHX_Filesystem_Open(PHX_Context* context, PHX_Disk_Device* device, PHX_Filesystem_Device* filesystemOut)
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
