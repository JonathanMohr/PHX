#include "partition.h"

#include "partition/mbr.h"
#include "partition/gpt.h"

PHX_Partition_Interface* PHX_Partition_Interfaces[] = {
    &PHX_Partition_MBR_Interface,
    &PHX_Partition_GPT_Interface
};
PHX_Size PHX_Partition_InterfaceCount = sizeof(PHX_Partition_Interfaces) / sizeof(PHX_Partition_Interfaces[0]);
