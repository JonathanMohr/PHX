#include "vdi.h"

static PHX_Bool PHX_VDI_GetDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    (void)context;
    (void)device;
    (void)readonly;
    (void)out;
    return PHX_FALSE;
}

static PHX_Bool PHX_VDI_FormatDevice(PHX_Context* context, PHX_BlockDevice* device, PHX_Bool readonly, PHX_BlockDevice* out)
{
    (void)context;
    (void)device;
    (void)readonly;
    (void)out;
    return PHX_FALSE;
}

PHX_Disk_Interface PHX_VDI_Interface = {
    PHX_VDI_GetDevice,
    PHX_VDI_FormatDevice,
    "VDI-DISK",
    "VDI-DISK-INTERFACE"
};
