#include "vhd.h"

static PHX_Bool PHX_VHD_GetDevice(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out)
{
    (void)context;
    (void)device;
    (void)readonly;
    (void)out;
    return PHX_FALSE;
}

static PHX_Bool PHX_VHD_FormatDevice(PHX_Context* context, PHX_File_Device* device, PHX_Bool readonly, PHX_Disk_Device* out)
{
    (void)context;
    (void)device;
    (void)readonly;
    (void)out;
    return PHX_FALSE;
}

PHX_Disk_Interface PHX_VHD_Interface = {
    PHX_VHD_GetDevice,
    PHX_VHD_FormatDevice,
    "VHD-DISK-INTERFACE",
    "vhd"
};
