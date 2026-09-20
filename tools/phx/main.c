#include <stdio.h>
#include <stdlib.h>

#include "device/device.h"
#include "file.h"
#include "partition/partition.h"
#include "types.h"

static void* PHX_Allocate(struct PHX_Allocator* allocator, PHX_Size size)
{
    (void)allocator;
    return malloc(size);
}

static void* PHX_Reallocate(struct PHX_Allocator* allocator, void* oldPtr, PHX_Size newSize)
{
    (void)allocator;
    return realloc(oldPtr, newSize);
}

static void PHX_Free(struct PHX_Allocator* allocator, void* ptr)
{
    (void)allocator;
    free(ptr);
}

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    struct PHX_Allocator allocator = {
        PHX_Allocate,
        PHX_Reallocate,
        PHX_Free,
        NULL
    };

    PHX_Context context = {
        PHX_TRUE,
        allocator
    };

    const char* file = argv[1];

    if (PHX_Disk_InterfaceCount == 0)
    {
        fputs("No disk interface found\n", stderr);
        return 1;
    }

    if (PHX_Partition_InterfaceCount == 0)
    {
        fputs("No partition interface found\n", stderr);
        return 1;
    }

    PHX_Disk_Interface* diskInterface = PHX_Disk_Interfaces[0];
    PHX_Partition_Interface* partitionInterface = PHX_Partition_Interfaces[0];

    PHX_BlockDevice fileDevice;
    printf("Opening file device for %s\n", file);
    if (PHX_File_Open(file, PHX_FALSE, &fileDevice, 1024 * 1024 * 1024) != PHX_TRUE)
    {
        fprintf(stderr, "Could not open file %s\n", file);
        return 1;
    }

    PHX_BlockDevice diskDevice;
    printf("Formatting image with disk interface %s...\n", diskInterface->name);
    if (diskInterface->formatDevice(&context, &fileDevice, PHX_FALSE, &diskDevice) != PHX_TRUE)
    {
        fputs("Formatting failed\n", stderr);
        fileDevice.close(&fileDevice);
        return 1;
    }

    PHX_Partition_Table partitionTable;
    partitionInterface->getDefaultTable(&context, &diskDevice, &partitionTable);
    printf("Formatting image with partition interface %s...\n", partitionInterface->name);
    if (partitionInterface->writeTable(&context, &diskDevice, &partitionTable, PHX_NULL) != PHX_TRUE)
    {
        fputs("Formatting failed\n", stderr);
        diskDevice.close(&diskDevice);
        return 1;
    }

    
    diskDevice.close(&diskDevice);

    return 0;
}
