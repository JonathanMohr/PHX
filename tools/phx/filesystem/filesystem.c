#include "filesystem.h"

#include "fat/fat.h"

PHX_Filesystem_Interface* PHX_Filesystem_Interfaces[] = {
    &PHX_Filesystem_FAT_Interface
};

PHX_Size PHX_Filesystem_InterfaceCount = sizeof(PHX_Filesystem_Interfaces) / sizeof(PHX_Filesystem_Interfaces[0]);
