#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include "device/device.h"
#include "disk.h"
#include "partition/partition.h"
#include "filesystem/filesystem.h"

#include "file.h"
#include "result.h"
#include "types.h"

static void* PHX_Allocate(struct PHX_Allocator* allocator, PHX_Size size)
{
    (void)allocator;
    void* r = malloc(size);
    return r;
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

static void print_help(const char* name)
{
    FILE* stream = stderr;

    fprintf(stream, "Usage: %s <area/\"help\"> <command> [...]\n", name);


    fputs("\nArea \"disk\":\n", stream);
    fputs("  Commands:\n", stream);
    fputs("  > list                               List supported interfaces\n", stream);
    fputs("  > create <image> <type> <size>       Create a new disk image\n", stream);
    fputs("  > extract <image> <out>              Extract a disk image to a binary file\n", stream);
    fputs("  > info <image>                       Print information about the disk image\n", stream);


    fputs("\nArea \"partition\":\n", stream);
    fputs("  Commands:\n", stream);
    fputs("  > list                               List supported interfaces\n", stream);
    fputs("  > info <image>                       Print information about the partition table\n", stream);
    fputs("  > create <image> <format>            Create empty partition table\n", stream);
    fputs("  > add <image> <type> <start> <size>  Add partition to partition table\n", stream);
    fputs("  > remove <image> <index>             Remove partition from partition table\n", stream);
    fputs("  > bootsector <image> <file>          Set bootsector of partition table\n", stream);
    // TODO: fputs("  > signature <image> <signature>      Set signature of partition table\n", stream);
    
    fputs("  Types:\n", stream);
    fputs("  - unknown\n", stream);
    fputs("  - fat12\n", stream);
    fputs("  - fat16\n", stream);
    fputs("  - fat32\n", stream);
}

static int disk(PHX_Context* context, const char* executable, const char* commandStr, const int argCount, const char** args)
{
    PHX_DetailedResult detailedResult;

    int fixedArgCount;
    enum
    {
        PHX_COMMAND_DISK_CREATE,
        PHX_COMMAND_DISK_EXTRACT,
        PHX_COMMAND_DISK_INFO
    } command;

    if (strcmp(commandStr, "list") == 0)
    {
        for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
        {
            PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
            fprintf(stdout, "Interface %" PRIu64 ":\n  Name: %s\n  Type: %s\n", i, interface->name, interface->type);
        }

        return 0;
    }

    if (strcmp(commandStr, "create") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_DISK_CREATE;
    }
    else if (strcmp(commandStr, "extract") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_DISK_EXTRACT;
    }
    else if (strcmp(commandStr, "info") == 0)
    {
        fixedArgCount = 1;
        command = PHX_COMMAND_DISK_INFO;
    }
    else
    {
        print_help(executable);
        return 1;
    }

    if (argCount < fixedArgCount)
    {
        print_help(executable);
        return 1;
    }

    const char* imagePath = args[0];

    PHX_BlockSize imageSize = PHX_FILE_SIZE_NONE;
    if (command == PHX_COMMAND_DISK_CREATE)
    {
        const char* sizeStr = args[2];
        imageSize = 0;
        while (*sizeStr)
        {
            if (*sizeStr < '0' || *sizeStr > '9')
                break;

            const char currentDigit = *sizeStr - '0';
            // TODO: Check for overflow
            imageSize = imageSize * 10 + (PHX_BlockSize)currentDigit;
            sizeStr++;
        }

        if (imageSize == 0)
        {
            fputs("Cannot create a disk image with size 0\n", stderr);
            return 1;
        }
    }

    PHX_BlockDevice fileDevice;
    if ((detailedResult = PHX_File_Open(imagePath, PHX_FALSE, &fileDevice, imageSize)).code != PHX_SUCCESS)
    {
        fprintf(stderr, "Could not open file %s: %s\n", imagePath, detailedResult.msg);
        return 1;
    }

    switch (command)
    {
        case PHX_COMMAND_DISK_CREATE:
        {
            const char* imageType = args[1];
            PHX_Disk_Interface* interface = PHX_Disk_FindInterfaceByType(imageType);
            if (!interface)
            {
                fprintf(stderr, "Could not find disk interface for type \"%s\"\n", imageType);
                
                fileDevice.close(&fileDevice);
                return 1;
            }

            PHX_BlockDevice diskDevice;
            if (interface->formatDevice(context, &fileDevice, PHX_FALSE, &diskDevice) != PHX_TRUE)
            {
                fputs("Formatting failed", stderr);

                fileDevice.close(&fileDevice);
                return 1;
            }

            diskDevice.close(&diskDevice);
            break;
        }

        case PHX_COMMAND_DISK_EXTRACT:
        {
            PHX_Result result;

            const char* outPath = args[1];

            PHX_BlockDevice diskDevice;
            if ((result = PHX_Disk_Open(context, &fileDevice, &diskDevice)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of disk image\n", stderr);
                else
                    fputs("Error while opening\n", stderr);

                fileDevice.close(&fileDevice);
                return 1;
            }

            PHX_BlockDevice outFileDevice;
            // TODO: Check for overflow
            if ((detailedResult = PHX_File_Open(outPath, PHX_FALSE, &outFileDevice, diskDevice.blockCount * diskDevice.blockSize)).code != PHX_SUCCESS)
            {
                fprintf(stderr, "Could not open file %s: %s\n", imagePath, detailedResult.msg);

                diskDevice.close(&diskDevice);
                return 1;
            }

            const PHX_Size blocksPerRead = (diskDevice.blockSize >= 16384) ? 1 : (16384 / diskDevice.blockSize);
            PHX_Byte* buffer = context->allocator.allocate(&context->allocator, diskDevice.blockSize * blocksPerRead);
            if (!buffer)
            {
                fputs("Could not allocate buffer\n", stderr);

                outFileDevice.close(&outFileDevice);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_BlockSize currentBlock = 0;
            while (currentBlock < diskDevice.blockCount)
            {
                PHX_BlockSize chunk = ((currentBlock + blocksPerRead) > diskDevice.blockCount) ? (diskDevice.blockCount - currentBlock) : (blocksPerRead);
                if (diskDevice.read(&diskDevice, buffer, currentBlock, chunk) != chunk)
                {
                    fputs("Read error\n", stderr);

                    context->allocator.free(&context->allocator, buffer);
                    outFileDevice.close(&outFileDevice);
                    diskDevice.close(&diskDevice);
                    return 1;
                }
                if (outFileDevice.write(&outFileDevice, buffer, currentBlock * diskDevice.blockSize, diskDevice.blockSize * chunk) != diskDevice.blockSize * chunk)
                {
                    fputs("Write error\n", stderr);

                    context->allocator.free(&context->allocator, buffer);
                    outFileDevice.close(&outFileDevice);
                    diskDevice.close(&diskDevice);
                    return 1;
                }
                currentBlock += chunk;
            }

            context->allocator.free(&context->allocator, buffer);

            outFileDevice.close(&outFileDevice);
            diskDevice.close(&diskDevice);
            break;
        }

        case PHX_COMMAND_DISK_INFO:
        {
            PHX_Result result;

            PHX_BlockDevice diskDevice;
            if ((result = PHX_Disk_Open(context, &fileDevice, &diskDevice)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of disk image\n", stderr);
                else
                    fputs("Error while opening\n", stderr);

                fileDevice.close(&fileDevice);
                return 1;
            }

            fprintf(stdout, "%s (%s):\n", imagePath, diskDevice.type);
            fprintf(stdout, "  Block Size: %" PRIu64 "\n", diskDevice.blockSize);
            fprintf(stdout, "  Block Count: %" PRIu64 "\n", diskDevice.blockCount);

            diskDevice.close(&diskDevice);
            break;
        }
    }

    return 0;
}

static const char* partitionTypeStr(PHX_Partition_Type type)
{
    switch (type)
    {
        case PHX_PARTITION_FAT12: return "FAT12";
        case PHX_PARTITION_FAT16: return "FAT16";
        case PHX_PARTITION_FAT32: return "FAT32";
        
        case PHX_PARTITION_UNKNOWN: default:
            return "Unknown";
    }
}

static int partition(PHX_Context* context, const char* executable, const char* commandStr, const int argCount, const char** args)
{
    PHX_Result result;
    PHX_DetailedResult detailedResult;

    int fixedArgCount;
    enum
    {
        PHX_COMMAND_PARTITION_INFO,
        PHX_COMMAND_PARTITION_CREATE,
        PHX_COMMAND_PARTITION_ADD,
        PHX_COMMAND_PARTITION_REMOVE,
        PHX_COMMAND_PARTITION_BOOTSECTOR,
    } command;

    if (strcmp(commandStr, "list") == 0)
    {
        for (PHX_Size i = 0; i < PHX_Partition_InterfaceCount; i++)
        {
            PHX_Partition_Interface* interface =PHX_Partition_Interfaces[i];
            fprintf(stdout, "Interface %" PRIu64 ":\n  Name: %s\n  Type: %s\n", i, interface->name, interface->type);
        }

        return 0;
    }

    if (strcmp(commandStr, "info") == 0)
    {
        fixedArgCount = 1;
        command = PHX_COMMAND_PARTITION_INFO;
    }
    else if (strcmp(commandStr, "create") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_PARTITION_CREATE;
    }
    else if (strcmp(commandStr, "add") == 0)
    {
        fixedArgCount = 4;
        command = PHX_COMMAND_PARTITION_ADD;
    }
    else if (strcmp(commandStr, "remove") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_PARTITION_REMOVE;
    }else if (strcmp(commandStr, "bootsector") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_PARTITION_BOOTSECTOR;
    }
    else
    {
        print_help(executable);
        return 1;
    }

    if (argCount < fixedArgCount)
    {
        print_help(executable);
        return 1;
    }

    const char* imagePath = args[0];

    PHX_BlockDevice fileDevice;
    if ((detailedResult = PHX_File_Open(imagePath, PHX_FALSE, &fileDevice, PHX_FILE_SIZE_NONE)).code != PHX_SUCCESS)
    {
        fprintf(stderr, "Could not open file %s: %s\n", imagePath, detailedResult.msg);
        return 1;
    }

    PHX_BlockDevice diskDevice;
    if ((result = PHX_Disk_Open(context, &fileDevice, &diskDevice)) != PHX_SUCCESS)
    {
        if (result == PHX_ERROR_FORMAT)
            fputs("Unknown format of disk image\n", stderr);
        else
            fputs("Error while opening\n", stderr);

        fileDevice.close(&fileDevice);
        return 1;
    }

    switch (command)
    {
        case PHX_COMMAND_PARTITION_INFO:
        {
            PHX_Partition_Interface* interface;
            PHX_Partition_Table table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &interface, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            fprintf(stdout, "%s (%s):\n", imagePath, interface->partitionType);
            fprintf(stdout, "  Usable start: %" PRIu64 "\n", table.startUsable);
            fprintf(stdout, "  Usable size: %" PRIu64 "\n", table.sizeUsable);
            fprintf(stdout, "  Signature: 0x%" PRIx32 "\n", table.signature);
            fprintf(stdout, "  Max Partition Count: %" PRIu64 "\n", table.maxPartitionCount);
            fprintf(stdout, "  Partition Count: %" PRIu64 "\n", table.partitionCount);
            fputs("  Partitions:\n", stdout);
            for (PHX_PartitionSize i = 0; i < table.partitionCount; i++)
            {
                PHX_Partition* partition = &table.partitions[i];
                fprintf(stdout, "    Partition %" PRIu64 "%s%s%s:\n", i + 1, (partition->name[0] != '\0') ? " (" : "", partition->name, (partition->name[0] != '\0') ? ")" : "");
                fprintf(stdout, "      Start: %" PRIu64 "\n", partition->start);
                fprintf(stdout, "      Size: %" PRIu64 "\n", partition->size);

                fputs("      Flags:", stdout);
                if (partition->flags | PHX_PARTITION_BOOTABLE)
                    fputs(" BOOTABLE", stdout);
                fputc('\n', stdout);

                fputs("      Type: ", stdout);
                fputs(partitionTypeStr(partition->type), stdout);
                fputc('\n', stdout);
            }

            PHX_Partition_CloseTable(context, &table);
            break;
        }

        case PHX_COMMAND_PARTITION_CREATE:
        {
            const char* type = args[1];
            PHX_Partition_Interface* interface = PHX_Partition_FindInterfaceByType(type);
            if (!interface)
            {
                fprintf(stderr, "Could not find disk interface for type \"%s\"\n", type);
                
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition_Table table;
            interface->getDefaultTable(context, &diskDevice, &table);

            if (interface->writeTable(context, &diskDevice, &table) != PHX_TRUE)
            {
                fputs("Error writing partition table to disk\n", stderr);

                PHX_Partition_CloseTable(context, &table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition_CloseTable(context, &table);
            break;
        }

        case PHX_COMMAND_PARTITION_ADD:
        {
            PHX_Partition_Interface* interface;
            PHX_Partition_Table table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &interface, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            if (table.partitionCount >= table.maxPartitionCount)
            {
                fputs("Limit of partitions reached for partition table\n", stderr);

                PHX_Partition_CloseTable(context, &table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            // TODO: Validate it is not overlapping

            PHX_Partition* newPartitions = context->allocator.reallocate(&context->allocator, table.partitions, sizeof(PHX_Partition) * (table.partitionCount + 1));
            if (!newPartitions)
            {
                fputs("Could not allocate new partition array\n", stderr);

                PHX_Partition_CloseTable(context, &table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            table.partitions = newPartitions;
            PHX_Partition* newPartition = &table.partitions[table.partitionCount++];

            newPartition->start = ...; // TODO
            newPartition->size = ...; // TODO

            newPartition->flags = 0;

            newPartition->type = partitionGetType(typeStr); // TODO

            memset(newPartition->name, '\0', sizeof(newPartition->name)); // TODO

            if (interface->writeTable(context, &diskDevice, &table) != PHX_TRUE)
            {
                fputs("Error writing partition table to disk\n", stderr);

                PHX_Partition_CloseTable(context, &table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition_CloseTable(context, &table);
            break;
        }

        default:
            break;
    }

    diskDevice.close(&diskDevice);
    return 0;
}

int main(int argc, const char* argv[])
{
    const char* executable = argv[0];

    if (argc < 2)
    {
        print_help(executable);
        return 1;
    }

    const char* area = argv[1];

    if (strcmp(area, "help") == 0 || strcmp(area, "-h") == 0)
    {
        print_help(executable);
        return 0;
    }

    if (argc < 3)
    {
        print_help(executable);
        return 1;
    }

    const char* command = argv[2];

    struct PHX_Allocator allocator = {
        PHX_Allocate,
        PHX_Reallocate,
        PHX_Free,
        NULL
    };

    PHX_Context context = {
        (PHX_u32)time(NULL),
        PHX_TRUE,
        allocator
    };
    
    if (strcmp(area, "disk") == 0)
        return disk(&context, executable, command, argc - 3, argv + 3);
    if (strcmp(area, "partition") == 0)
        return partition(&context, executable, command, argc - 3, argv + 3);
    
    print_help(executable);
    return 1;

    /*
    PHX_DetailedResult detailedResult;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

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
    if ((detailedResult = PHX_File_Open(file, PHX_FALSE, &fileDevice, 1024ull * 1024ull * 512ull)).code != PHX_SUCCESS)
    {
        fprintf(stderr, "Could not open file %s: %s\n", file, detailedResult.msg ? detailedResult.msg : "?");
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
    partition1->type = PHX_PARTITION_FAT32;
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
    if (partitionInterface->writeTable(&context, &diskDevice, &partitionTable) != PHX_TRUE)
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


    PHX_Filesystem_Node rootNode;
    if (filesystem.ops->getNode(&filesystem, filesystem.ops->rootNodeNumber, &rootNode) != PHX_SUCCESS)
    {
        fputs("Could not get root node\n", stderr);
        goto cleanup;
    }

    if (filesystem.ops->createNode(&filesystem, &rootNode, PHX_FILESYSTEM_ENTRY_FILE, 0, "test.txt", PHX_NULL) != PHX_SUCCESS)
    {
        fputs("Could not create test.txt in root\n", stderr);
        goto cleanup;
    }


cleanup:
    filesystem.ops->destroy(&filesystem);
    partitionDevice.close(&partitionDevice);
    PHX_Partition_CloseTable(&context, &partitionTable);
    diskDevice.close(&diskDevice);

    return 0;

    */
}
