#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "device/device.h"
#include "partition/partition.h"
#include "filesystem/filesystem.h"

#include "file.h"
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

    if (PHX_Filesystem_InterfaceCount == 0)
    {
        fputs("No filesystem interface found\n", stderr);
        return 1;
    }

    PHX_Disk_Interface* diskInterface = PHX_Disk_Interfaces[0];
    PHX_Partition_Interface* partitionInterface = PHX_Partition_Interfaces[0];
    PHX_Filesystem_Interface* filesystemInterface = PHX_Filesystem_Interfaces[0];


    PHX_BlockDevice fileDevice;
    printf("Opening file device for %s...\n", file);
    if (PHX_File_Open(file, PHX_FALSE, &fileDevice, 1024 * 1024 * 1024 / 4) != PHX_TRUE)
    {
        fprintf(stderr, "Could not open file %s\n", file);
        return 1;
    }

    PHX_BlockDevice realisticFileDevice;
    puts("Creating realistic file device...");
    if (PHX_BlockCountTransformDevice(&context, &fileDevice, 512, PHX_TRUE, &realisticFileDevice) != PHX_TRUE)
    {
        fputs("Creating realistic file device failed\n", stderr);
        fileDevice.close(&fileDevice);
        return 1;
    }

    PHX_BlockDevice diskDevice;
    printf("Formatting image with disk interface %s...\n", diskInterface->name);
    if (diskInterface->formatDevice(&context, &realisticFileDevice, PHX_FALSE, &diskDevice) != PHX_TRUE)
    {
        fputs("Formatting failed\n", stderr);
        realisticFileDevice.close(&realisticFileDevice);
        return 1;
    }

    PHX_Partition_Table partitionTable;
    partitionInterface->getDefaultTable(&context, &diskDevice, &partitionTable);
    printf("Getting empty partition table with partition interface %s...\n", partitionInterface->name);

    puts("Creating partition 1...");
    partitionTable.partitions = context.allocator.allocate(&context.allocator, sizeof(PHX_Partition));
    if (!partitionTable.partitions)
    {
        fputs("Could not allocate partition\n", stderr);
        diskDevice.close(&diskDevice);
        return 1;
    }
    PHX_Partition* partition1 = &partitionTable.partitions[0];
    partition1->start = partitionTable.startUsable;
    partition1->size = partitionTable.sizeUsable;
    partition1->flags = PHX_PARTITION_BOOTABLE;
    partition1->type = PHX_PARTITION_UNKNOWN;
    memset(partition1->name, '\0', sizeof(partition1->name));

    partitionTable.partitionCount = 1;


    PHX_BlockDevice partitionDevice;
    puts("Creating device for partition 1...");
    if (PHX_Partition_CreateDevice(&context, &diskDevice, partition1, &partitionDevice) != PHX_TRUE)
    {
        fputs("Creating device failed\n", stderr);
        diskDevice.close(&diskDevice);
        return 1;
    }

    printf("Writting partition table with partition interface %s...\n", partitionInterface->name);
    if (partitionInterface->writeTable(&context, &diskDevice, &partitionTable, PHX_NULL) != PHX_TRUE)
    {
        fputs("Formatting failed\n", stderr);
        partitionDevice.close(&partitionDevice);
        diskDevice.close(&diskDevice);
        return 1;
    }


    PHX_Filesystem filesystem;
    printf("Formatting partition with filesystem interface %s...\n", filesystemInterface->name);
    if (filesystemInterface->formatFilesystem(&context, &partitionDevice, &filesystem, PHX_NULL) != PHX_SUCCESS)
    {
        fputs("Formatting failed\n", stderr);
        partitionDevice.close(&partitionDevice);
        diskDevice.close(&diskDevice);
        return 1;
    }


    filesystem.ops->destroy(&filesystem);
    partitionDevice.close(&partitionDevice);
    PHX_Partition_CloseTable(&context, &partitionTable);
    diskDevice.close(&diskDevice);

    return 0;
}
