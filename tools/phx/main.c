#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>

#include <phx/native/filesystem.h>
#include <phx/native/memory.h>

#include <phx/utility/partition.h>
#include <phx/utility/filesystem.h>

#include "interfaces.h"
#include "disk/disk.h"
#include "partition/partition.h"
#include "filesystem/filesystem.h"
#include "phx/device/partition.h"
#include "phx/interface/partition.h"

static void print_help(const char* name);


static PHX_AnySize PHX_Size_FromStr(const char* str, PHX_Bool* overflow, PHX_AnySize max)
{
    if (!str) return 0;

    const char* strPtr = str;
    while (*strPtr)
        strPtr++;

    PHX_Size len = (PHX_Size)(strPtr - str);
    if (len == 0) return 0;

    PHX_AnySize base = 1024ULL;
    if (str[len - 1] == 'd' || str[len - 1] == 'D')
    {
        base = 1000ULL;
        len--;
    }

    if (len == 0) return 0;

    if (str[len - 1] == 'b' || str[len - 1] == 'B')
        len--;

    if (len == 0) return 0;

    PHX_AnySize multiplier = 1;
    switch (str[len - 1])
    {
        case 't': case 'T':
            multiplier = base * base * base * base;
            len--;
            break;

        case 'g': case 'G':
            multiplier = base * base * base;
            len--;
            break;

        case 'm': case 'M':
            multiplier = base * base;
            len--;
            break;

        case 'k': case 'K':
            multiplier = base;
            len--;
            break;
    }

    PHX_AnySize whole = 0;
    PHX_Size i = 0;
    while (i < len && str[i] >= '0' && str[i] <= '9')
    {
        PHX_AnySize digit = (PHX_AnySize)(unsigned char)(str[i] - '0');
        if (digit > max || whole > (max - digit) / 10)
        {
            *overflow = PHX_TRUE;
            return 0;
        }
        
        whole = whole * 10 + digit;
        i++;
    }

    PHX_Size fracBegin = i;
    if (i < len && str[i] == '.')
    {
        fracBegin = ++i;
        while (i < len && str[i] >= '0' && str[i] <= '9')
            i++;
    }

    if (whole > max / multiplier)
    {
        *overflow = PHX_TRUE;
        return 0;
    }
    PHX_AnySize result = whole * multiplier;

    PHX_AnySize frac = 0;
    while (i-- > fracBegin)
    {
        PHX_AnySize digit = (PHX_AnySize)(unsigned char)(str[i] - '0');
        frac = (digit * multiplier + frac) / 10;
    }

    if (frac > max - result)
    {
        *overflow = PHX_TRUE;
        return 0;
    }

    *overflow = PHX_FALSE;
    return result + frac;
}
#define PHX_Size_FromStr(str, overflow, type) ((type)PHX_Size_FromStr(str, overflow, (PHX_AnySize)(type)~(type)0))

static PHX_Disk_Device* PHX_Device_FromStr(PHX_Context* context, const char* str, PHX_File_Device* fileDevice, PHX_Disk_Device* diskOut, PHX_Disk_Device* partitionOut)
{
    PHX_Result result;
    PHX_DetailedResult detailedResult;

    const char* strPtr = str;
    const char* lastColon = PHX_NULL;
    while (*strPtr)
    {
        if (*strPtr == ':') lastColon = strPtr;
        strPtr++;
    }

    if (lastColon)
    {
        strPtr = lastColon + 1;
        while (*strPtr >= '0' && *strPtr <= '9')
            strPtr++;

        if (*strPtr || strPtr == lastColon + 1)
            lastColon = PHX_NULL;
    }

    const char* name = lastColon ? context->allocator->allocate(context->allocator, (PHX_Size)(lastColon - str + 1)) : str;
    if (lastColon && !name)
    {
        fputs("Could not allocate memory for name\n", stderr);
        return PHX_NULL;
    }

    if (lastColon)
    {
        memcpy((char*)name, str, (PHX_Size)(lastColon - str));
        ((char*)name)[(PHX_Size)(lastColon - str)] = '\0';
    }

    if ((detailedResult = PHX_Native_File_Open(lastColon ? name : str, PHX_NATIVE_FILE_MODE_READ_WRITE, fileDevice, 0)).code != PHX_SUCCESS)
    {
        fprintf(stderr, "Could not open file %s: %s\n", name, detailedResult.msg);

        if (lastColon)
            context->allocator->free(context->allocator, (char*)name);
        return PHX_NULL;
    }

    if (lastColon)
        context->allocator->free(context->allocator, (char*)name);
    
    if ((result = PHX_Disk_Open(context, fileDevice, diskOut)) != PHX_SUCCESS)
    {
        if (result == PHX_ERROR_FORMAT)
            fputs("Unknown format of disk image\n", stderr);
        else
            fputs("Error while opening\n", stderr);

        PHX_Native_File_Close(fileDevice);
        return PHX_NULL;
    }

    if (!lastColon)
        return diskOut;

    PHX_Partition_Table_Device table;
    if ((result = PHX_Partition_Open(context, diskOut, &table)) != PHX_SUCCESS)
    {
        if (result == PHX_ERROR_FORMAT)
            fputs("Unknown format of partition table\n", stderr);
        else
            fputs("Error while reading partition table\n", stderr);

        diskOut->close(diskOut);
        return PHX_NULL;
    }

    const char* indexStr = lastColon + 1;
    PHX_Bool overflow;
    PHX_PartitionSize index = PHX_Size_FromStr(indexStr, &overflow, PHX_PartitionSize); // TODO: Better

    if (overflow == PHX_TRUE)
    {
        fputs("Overflow while reading index\n", stderr);

        table.ops->close(&table);
        diskOut->close(diskOut);
        return PHX_NULL;
    }

    if (index == 0 || index - 1 >= table.partitionSlots)
    {
        fputs("Index out of bounds\n", stderr);

        table.ops->close(&table);
        diskOut->close(diskOut);
        return PHX_NULL;
    }

    PHX_Partition* partition = PHX_Partition_Table_GetPartition(&table, index - 1);
    if (partition->type == PHX_PARTITION_TYPE_EMPTY)
    {
        fprintf(stderr, "Partition %" PRIu64 " is empty\n", index);

        table.ops->close(&table);
        diskOut->close(diskOut);
        return PHX_NULL;
    }

    if (PHX_Partition_CreateDevice(context, diskOut, partition, partitionOut) != PHX_TRUE)
    {
        fputs("Error while creating partition device\n", stderr);

        table.ops->close(&table);
        diskOut->close(diskOut);
        return PHX_NULL;
    }

    table.ops->close(&table);
    return partitionOut;
}


static void print_signature(FILE* stream, PHX_Partition_Signature* signature)
{
    switch (signature->type)
    {
        case PHX_PARTITION_SIGNATURE_NONE:
            fputs("NO SIGNATURE", stream);
            break;
        
        case PHX_PARTITION_SIGNATURE_FIRST:
            fprintf(stream, "0x%" PRIX64, signature->first);
            break;

        case PHX_PARTITION_SIGNATURE_GUID:
            fprintf(
                stream,
                "%08" PRIX32 "-%04" PRIX16 "-%04" PRIX16 "-%04" PRIX16 "-%012" PRIX64,
                (PHX_u32)(signature->second >> 32),
                (PHX_u16)((signature->second >> 16) & 0xFFFF),
                (PHX_u16)(signature->second & 0xFFFF),
                (PHX_u16)(signature->first >> 48),
                (PHX_u64)(signature->first & 0xFFFFFFFFFFFF)
            );
            break;

        default:
            fputs("INVALID SIGNATURE TYPE", stream);
            break;
    }
}


static int disk(PHX_Context* context, const char* executable, const char* commandStr, const int argCount, const char** args)
{
    PHX_DetailedResult detailedResult;

    int fixedArgCount;
    enum
    {
        PHX_COMMAND_DISK_CREATE,
        PHX_COMMAND_DISK_INFO
    } command;

    if (strcmp(commandStr, "list") == 0)
    {
        for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
        {
            PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
            fprintf(stdout, "Interface %zu:\n  Name: %s\n  Type: %s\n", i + 1, interface->name, interface->type);
        }

        return 0;
    }

    if (strcmp(commandStr, "create") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_DISK_CREATE;
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

    PHX_FileSize imageSize = 0;
    if (command == PHX_COMMAND_DISK_CREATE)
    {
        const char* sizeStr = args[2];

        PHX_Bool overflow;
        imageSize = PHX_Size_FromStr(sizeStr, &overflow, PHX_FileSize);
        
        if (overflow == PHX_TRUE)
        {
            fputs("Overflow of size\n", stderr);
            return 1;
        }

        if (imageSize == 0)
        {
            fputs("Cannot create a disk image with size 0\n", stderr);
            return 1;
        }
    }

    const PHX_Native_File_Mode imageMode = (command == PHX_COMMAND_DISK_CREATE) ? PHX_NATIVE_FILE_MODE_CREATE : PHX_NATIVE_FILE_MODE_READ_WRITE;

    PHX_File_Device fileDevice;
    if ((detailedResult = PHX_Native_File_Open(imagePath, imageMode, &fileDevice, imageSize)).code != PHX_SUCCESS)
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
                
                PHX_Native_File_Close(&fileDevice);
                return 1;
            }

            PHX_Disk_Device diskDevice;
            if (interface->formatDevice(context, &fileDevice, PHX_FALSE, &diskDevice) != PHX_TRUE)
            {
                fputs("Formatting failed\n", stderr);

                PHX_Native_File_Close(&fileDevice);
                return 1;
            }

            diskDevice.close(&diskDevice);
            break;
        }

        case PHX_COMMAND_DISK_INFO:
        {
            PHX_Result result;

            PHX_Disk_Device diskDevice;
            if ((result = PHX_Disk_Open(context, &fileDevice, &diskDevice)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of disk image\n", stderr);
                else
                    fputs("Error while opening\n", stderr);

                PHX_Native_File_Close(&fileDevice);
                return 1;
            }

            fprintf(stdout, "%s (%s):\n", imagePath, diskDevice.type);
            fprintf(stdout, "  Sector Size: %" PRIu64 "\n", diskDevice.sectorSize);
            fprintf(stdout, "  Total Sectors: %" PRIu64 "\n", diskDevice.totalSectors);

            fprintf(stdout, "  Total Cylinders: %" PRIu64 "\n", diskDevice.totalCylinders);
            fprintf(stdout, "  Total Heads: %" PRIu8 "\n", diskDevice.totalHeads);
            fprintf(stdout, "  Sectors Per Track: %" PRIu8 "\n", diskDevice.sectorsPerTrack);

            diskDevice.close(&diskDevice);
            break;
        }
    }

    return 0;
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
            PHX_Partition_Interface* interface = PHX_Partition_Interfaces[i];
            fprintf(stdout, "Interface %zu:\n  Name: %s\n  Type: %s\n", i + 1, interface->name, interface->type);
        }

        return 0;
    }

    if (strcmp(commandStr, "types") == 0)
    {
        if (argCount < 1)
        {
            print_help(executable);
            return 1;
        }
        
        const char* format = args[0];
        PHX_Partition_Interface* interface = PHX_Partition_FindInterfaceByType(format);
        if (!interface)
        {
            fputs("Unknown format ", stderr);
            fputs(format, stderr);
            fputc('\n', stderr);
        }

        for (PHX_Size i = 0; i < interface->ops->typeCount; i++)
        {
            PHX_Partition_Type_Info* typeInfo = &interface->ops->typeInfos[i];

            const char** names = typeInfo->names;
            fputs(" - ", stdout);
            while (*names)
            {
                if (names != typeInfo->names)
                    fprintf(stdout, " / %s", *names);
                else
                    fputs(*names, stdout);
                names++;
            }
            fputc('\n', stdout);
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

    // Flags
    PHX_Bool flags_bootable = PHX_FALSE;
    const char* flags_type = PHX_NULL;
    for (int i = fixedArgCount; i < argCount; i++)
    {
        if (strcmp(args[i], "--bootable") == 0)
        {
            if (flags_bootable == PHX_TRUE)
            {
                fputs("Warning: --bootable used multiple times\n", stderr);
                continue;
            }

            flags_bootable = PHX_TRUE;
        }
        else if (strcmp(args[i], "--type") == 0)
        {
            if (flags_type)
            {
                fputs("Warning: --type used multiple times\n", stderr);
                continue;
            }

            if (i + 1 >= argCount)
                fputs("Warning: No argument after --type\n", stderr);
            else
                flags_type = args[++i];
        }
        else
        {
            fputs("Warning: Invalid argument: ", stderr);
            fputs(args[i], stderr);
            fputc('\n', stderr);
        }
    }

    PHX_File_Device fileDevice;
    if ((detailedResult = PHX_Native_File_Open(imagePath, PHX_NATIVE_FILE_MODE_READ_WRITE, &fileDevice, 0)).code != PHX_SUCCESS)
    {
        fprintf(stderr, "Could not open file %s: %s\n", imagePath, detailedResult.msg);
        return 1;
    }

    PHX_Disk_Device diskDevice;
    if ((result = PHX_Disk_Open(context, &fileDevice, &diskDevice)) != PHX_SUCCESS)
    {
        if (result == PHX_ERROR_FORMAT)
            fputs("Unknown format of disk image\n", stderr);
        else
            fputs("Error while opening\n", stderr);

        PHX_Native_File_Close(&fileDevice);
        return 1;
    }

    switch (command)
    {
        case PHX_COMMAND_PARTITION_INFO:
        {
            PHX_Partition_Table_Device table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            char* specificType = context->allocator->allocate(context->allocator, table.ops->specificTypeLen);
            if (!specificType)
            {
                fputs("Could not allocate memory for specific name\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            fprintf(stdout, "%s (%s):\n", imagePath, table.type);
            fprintf(stdout, "  Usable start: %" PRIu64 " (Sector: %" PRIu64 ")\n", table.startUsable * diskDevice.sectorSize, table.startUsable);
            fprintf(stdout, "  Usable size: %" PRIu64 " (Sectors: %" PRIu64 ")\n", table.sizeUsable * diskDevice.sectorSize, table.sizeUsable);
            fputs("  Signature: ", stdout);
            print_signature(stdout, &table.signature);
            fputc('\n', stdout);
            fprintf(stdout, "  Partition Slots: %" PRIu64 "\n", table.partitionSlots);
            fputs("  Partitions:\n", stdout);
            for (PHX_PartitionSize i = 0; i < table.partitionSlots; i++)
            {
                PHX_Partition* partition = PHX_Partition_Table_GetPartition(&table, i);

                const char* typeName = table.ops->getTypeName(&table, i);
                table.ops->getSpecificType(&table, i, specificType);

                if (partition->name[0] == '\0')
                    fprintf(stdout, "    Partition %" PRIu64 ": ", i + 1);
                else
                    fprintf(stdout, "    Partition %" PRIu64 " \"%s\": ", i + 1, partition->name);

                if (partition->type == PHX_PARTITION_TYPE_EMPTY)
                {
                    fputs("Empty\n", stdout);
                    continue;
                }
                else
                    fputc('\n', stdout);

                fprintf(stdout, "      Start: %" PRIu64 " (Sector: %" PRIu64 ")\n", partition->start * diskDevice.sectorSize, partition->start);
                fprintf(stdout, "      Size: %" PRIu64 " (Sectors: %" PRIu64 ")\n", partition->size * diskDevice.sectorSize, partition->size);
                if (typeName)
                    fprintf(stdout, "      Type: %s (%s)\n", typeName, specificType);
                else
                    fprintf(stdout, "      Type: %s\n", specificType);

                fputs("      Signature: ", stdout);
                print_signature(stdout, &partition->signature);
                fputc('\n', stdout);

                // TODO: Flags
            }

            context->allocator->free(context->allocator, specificType);
            table.ops->close(&table);
            break;
        }

        case PHX_COMMAND_PARTITION_CREATE:
        {
            const char* format = args[1];
            PHX_Partition_Interface* interface = PHX_Partition_FindInterfaceByType(format);
            if (!interface)
            {
                fprintf(stderr, "Could not find partition interface for format \"%s\"\n", format);
                
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition_Table_Device table;
            if ((result = interface->formatPartitionTable(context, &diskDevice, &table, PHX_NULL)) != PHX_SUCCESS)
            {
                fputs("Error formatting partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            if ((result = table.ops->writeChanges(&table)) != PHX_SUCCESS)
            {
                fputs("Error writing partition table to disk\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            table.ops->close(&table);
            break;
        }

        case PHX_COMMAND_PARTITION_ADD:
        {
            const char* indexStr = args[1];
            const char* startStr = args[2];
            const char* sizeStr = args[3];

            PHX_Bool overflow;

            PHX_Partition_Table_Device table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Size index = PHX_Size_FromStr(indexStr, &overflow, PHX_Size);
            if (overflow == PHX_TRUE)
            {
                fputs("Overflow of index\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            if (index >= table.partitionSlots)
            {
                fputs("Index out of bounds\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition* partition = PHX_Partition_Table_GetPartition(&table, index - 1);
            if (partition->type != PHX_PARTITION_TYPE_EMPTY)
            {
                fprintf(stderr, "Partition slot %" PRIu64 " is not empty\n", index);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            // TODO: Validate it is not overlapping

            PHX_SectorSize start = PHX_Size_FromStr(startStr, &overflow, PHX_SectorSize);
            if (overflow == PHX_TRUE)
            {
                fputs("Overflow of start\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_SectorSize size = PHX_Size_FromStr(sizeStr, &overflow, PHX_SectorSize);
            if (overflow == PHX_TRUE)
            {
                fputs("Overflow of size\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            if (start % diskDevice.sectorSize)
            {
                fputs("Start not aligned to sector size\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }
            if (size % diskDevice.sectorSize)
            {
                fputs("Size not aligned to sector size\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            // TODO
            partition->signature.first = 0;
            partition->signature.second = 0;
            partition->signature.type = PHX_PARTITION_SIGNATURE_NONE;

            partition->start = start / diskDevice.sectorSize;
            partition->size = size / diskDevice.sectorSize;

            partition->flags = PHX_PARTITION_FLAGS_CHANGED;
            if (flags_bootable == PHX_TRUE)
                partition->flags |= PHX_PARTITION_FLAGS_LEGACY_BOOTABLE;

            partition->type = PHX_PARTITION_TYPE_UNKNOWN; // TODO
            if (flags_type)
            {
                if (table.ops->setType(&table, index - 1, flags_type) != PHX_TRUE)
                {
                    fputs("Invalid type: ", stderr);
                    fputs(flags_type, stderr);
                    fputc('\n', stderr);

                    table.ops->close(&table);
                    diskDevice.close(&diskDevice);
                    return 1;
                }
            }

            memset(partition->name, '\0', sizeof(partition->name)); // TODO

            if ((result = table.ops->writeChanges(&table)) != PHX_SUCCESS)
            {
                fputs("Error writing partition table to disk\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            table.ops->close(&table);
            break;
        }

        case PHX_COMMAND_PARTITION_REMOVE:
        {
            PHX_Partition_Table_Device table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Bool overflow;

            const char* indexStr = args[1];


            // TODO: Better
            PHX_Size index = PHX_Size_FromStr(indexStr, &overflow, PHX_Size);
            if (overflow == PHX_TRUE)
            {
                fputs("Overflow of index\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            if (index >= table.partitionSlots)
            {
                fputs("Index out of bounds\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Partition* partition = PHX_Partition_Table_GetPartition(&table, index - 1);
            if (partition->type == PHX_PARTITION_TYPE_EMPTY)
            {
                fprintf(stderr, "Partition slot %" PRIu64 " is already empty\n", index);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            partition->type = PHX_PARTITION_TYPE_EMPTY;
            partition->flags = PHX_PARTITION_FLAGS_CHANGED;

            if ((result = table.ops->writeChanges(&table)) != PHX_SUCCESS)
            {
                fputs("Error writing partition table to disk\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            table.ops->close(&table);
            break;
        }

        case PHX_COMMAND_PARTITION_BOOTSECTOR:
        {
            const char* bootsectorFileStr = args[1];

            PHX_Partition_Table_Device table;
            if ((result = PHX_Partition_Open(context, &diskDevice, &table)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_FORMAT)
                    fputs("Unknown format of partition table\n", stderr);
                else
                    fputs("Error while reading partition table\n", stderr);

                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_File_Device bootsectorFileDevice;
            if ((detailedResult = PHX_Native_File_Open(bootsectorFileStr, PHX_NATIVE_FILE_MODE_READ, &bootsectorFileDevice, 0)).code != PHX_SUCCESS)
            {
                fprintf(stderr, "Could not open file %s: %s\n", bootsectorFileStr, detailedResult.msg);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_u8 bootsectorBuffer[512] = {0};

            if (PHX_Native_File_Read(&bootsectorFileDevice, bootsectorBuffer, 0, 512) != 512)
            {
                fputs("Error reading 512 bytes from bootsector file\n", stderr);

                PHX_Native_File_Close(&bootsectorFileDevice);
                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_Native_File_Close(&bootsectorFileDevice);

            if ((result = table.ops->changeBootsector(&table, bootsectorBuffer)) != PHX_SUCCESS)
            {
                fputs("Error reading 512 bytes from bootsector file\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            if ((result = table.ops->writeChanges(&table)) != PHX_SUCCESS)
            {
                fputs("Error writing partition table to disk\n", stderr);

                table.ops->close(&table);
                diskDevice.close(&diskDevice);
                return 1;
            }

            table.ops->close(&table);
            break;
        }

        default:
            break;
    }

    diskDevice.close(&diskDevice);
    return 0;
}


struct PHX_Entry_Info
{
    PHX_Filesystem_Entry entry;
    PHX_Filesystem_Node node;
};

static int entry_type_rank(PHX_Filesystem_Node_Type type)
{
    switch (type)
    {
        case PHX_FILESYSTEM_NODE_TYPE_DIRECTORY: return 0;
        case PHX_FILESYSTEM_NODE_TYPE_FILE: return 1;
        default: return 2;
    }
}

static int compareEntryInfo(const void* a, const void* b)
{
    const struct PHX_Entry_Info* entryA = a;
    const struct PHX_Entry_Info* entryB = b;

    int ra = entry_type_rank(entryA->node.type);
    int rb = entry_type_rank(entryB->node.type);

    if (ra != rb)
        return (ra > rb) - (ra < rb);

    return strcmp(entryA->entry.name, entryB->entry.name);
}

static void printEntry(const struct PHX_Entry_Info* entryInfo, FILE* stream)
{
    const PHX_Filesystem_Entry* entry = &entryInfo->entry;
    const PHX_Filesystem_Node* node = &entryInfo->node;

    switch (node->type)
    {
        case PHX_FILESYSTEM_NODE_TYPE_FILE:
            fputs("fil", stream);
            break;

        case PHX_FILESYSTEM_NODE_TYPE_DIRECTORY:
            fputs("dir", stream);
            break;

        default:
            fputs("inv", stream);
            break;
    }

    fputs(" (", stream);

    if (node->attributes & PHX_FILESYSTEM_NODE_ATTRIBUTE_READONLY)
        fputs("ro | ", stream);
    else
        fputs("-- | ", stream);
    if (node->attributes & PHX_FILESYSTEM_NODE_ATTRIBUTE_EXECUTABLE)
        fputs("exec | ", stream);
    else
        fputs("---- | ", stream);
    if (node->attributes & PHX_FILESYSTEM_NODE_ATTRIBUTE_HIDDEN)
        fputs("hid | ", stream);
    else
        fputs("--- | ", stream);
    if (node->attributes & PHX_FILESYSTEM_NODE_ATTRIBUTE_SYSTEM)
        fputs("sys", stream);
    else
        fputs("---", stream);

    fputs(") ", stream);

    fprintf(stream, "[%" PRIu64 "] ", node->size);

    fputs(entry->name, stream);

    fputc('\n', stream);
}

static int printNode(PHX_Filesystem_Device* filesystem, PHX_Filesystem_Node* dir, PHX_Bool recursive)
{
    // TODO: Make recursive look nicer and more like tree

    PHX_Result result;

    PHX_u64 entryCount;
    if ((result = filesystem->ops->dir_getEntryCount(filesystem, dir, &entryCount)) != PHX_SUCCESS)
    {
        fputs("Could not get count of children\n", stderr);
        return 1;
    }

    if (entryCount == 0)
        return 0;

    struct PHX_Entry_Info* entries = filesystem->context->allocator->allocate(filesystem->context->allocator, sizeof(struct PHX_Entry_Info) * entryCount);
    if (!entries)
    {
        fputs("Could not allocate memory for entries\n", stderr);
        return 1;
    }

    PHX_Filesystem_OpenNode openNode;
    if ((result = filesystem->ops->createOpenNode(filesystem, dir, &openNode)) != PHX_SUCCESS)
    {
        fputs("Could not create open node\n", stderr);
        filesystem->context->allocator->free(filesystem->context->allocator, entries);
        return 1;
    }


    PHX_Size currentEntry = 0;
    while(currentEntry < entryCount && (result = filesystem->ops->dir_readEntry(filesystem, dir, &openNode, &entries[currentEntry].entry)) == PHX_SUCCESS)
    {
        if ((result = filesystem->ops->getNode(filesystem, entries[currentEntry].entry.node, &entries[currentEntry].node)) != PHX_SUCCESS)
            break;
        currentEntry++;
    }

    if (result != PHX_SUCCESS && result != PHX_ERROR_NOT_FOUND)
    {
        fputs("Error while reading directory entries\n", stderr);
        for (PHX_Size i = 0; i < currentEntry; i++)
            filesystem->ops->cleanupNode(filesystem, &entries[i].node);
        filesystem->context->allocator->free(filesystem->context->allocator, entries);
        return 1;
    }

    qsort(entries, currentEntry, sizeof(entries[0]), compareEntryInfo);

    for (PHX_Size i = 0; i < currentEntry; i++)
        printEntry(&entries[i], stdout);

    if (recursive == PHX_TRUE)
    {
        for (PHX_Size i = 0; i < currentEntry; i++)
        {
            struct PHX_Entry_Info* entryInfo = &entries[i];
            if (entryInfo->node.type != PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
                continue;

            fprintf(stdout, "\n%s:\n", entryInfo->entry.name);
            const int returnCode = printNode(filesystem, &entryInfo->node, PHX_TRUE);
            if (returnCode != 0)
                return returnCode;
        }
    }

    for (PHX_Size i = 0; i < currentEntry; i++)
        filesystem->ops->cleanupNode(filesystem, &entries[i].node);
    (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
    filesystem->context->allocator->free(filesystem->context->allocator, entries);

    return 0;
}

static int isSafeName(const char* name)
{
    if (name[0] == '\0') return 0;
    if (name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
        return 0;
    for (const char* p = name; *p; p++)
        if (*p == '/' || *p == '\\' || *p == ':')
            return 0;
    return 1;
}

static void printSafe(FILE* stream, const char* s)
{
    for (; *s; s++)
    {
        unsigned char c = (unsigned char)*s;
        if (c < 0x20 || c == 0x7F)
            fprintf(stream, "\\x%02x", c);
        else
            fputc(c, stream);
    }
}

static int extract(PHX_Filesystem_Device* filesystem, PHX_Filesystem_Node* node, const char* hostPath, PHX_Size maxRecursionDepth, PHX_Size currentRecursionDepth)
{
    PHX_Result result;
    PHX_DetailedResult detailedResult;

    const char* hostPathEnd = hostPath;
    while (*hostPathEnd) hostPathEnd++;
    const PHX_Size hostPathLen = (PHX_Size)(hostPathEnd - hostPath);

    if (hostPathLen == 0)
    {
        fputs("Invalid host path\n", stderr);
        return 1;
    }

    // TODO: Attributes

    if (node->type == PHX_FILESYSTEM_NODE_TYPE_FILE)
    {
        PHX_Filesystem_OpenNode openNode;
        if ((result = filesystem->ops->createOpenNode(filesystem, node, &openNode)) != PHX_SUCCESS)
        {
            fputs("Could not create open node\n", stderr);
            return 1;
        }

        PHX_File_Device fileDevice;
        if ((detailedResult = PHX_Native_File_Open(hostPath, PHX_NATIVE_FILE_MODE_CREATE, &fileDevice, node->size)).code != PHX_SUCCESS)
        {
            fprintf(stderr, "Could not open file %s: %s\n", hostPath, detailedResult.msg);
            (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
            return 1;
        }

        PHX_Byte buffer[4096];
        PHX_Filesystem_Node_Size written = 0;
        while (written < node->size)
        {
            const PHX_Filesystem_Node_Size remaining = node->size - written;
            const PHX_Filesystem_Node_Size chunk = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);

            PHX_Filesystem_Node_Size read = filesystem->ops->file_read(filesystem, node, &openNode, chunk, buffer);
            if (read == 0)
                break;
            PHX_Filesystem_Node_Size currentWritten = (PHX_Filesystem_Node_Size)PHX_Native_File_Write(&fileDevice, buffer, written, (PHX_FileSize)read);
            written += currentWritten;
            if (currentWritten != read)
                break;
        }

        PHX_Native_File_Close(&fileDevice);
        (void)filesystem->ops->closeOpenNode(filesystem, &openNode);

        if (written < node->size)
        {
            fputs("Error while reading content from file and writing it to host file\n", stderr);
            return 1;
        }
    }
    else if (node->type == PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
    {
        if (PHX_Native_Directory_Make(hostPath) != PHX_TRUE)
        {
            fprintf(stderr, "Error while creating directory %s\n", hostPath);
            return 1;
        }

        if (maxRecursionDepth != 0 && currentRecursionDepth >= maxRecursionDepth)
        {
            fputs("Reached max recursion depth\n", stderr);
        }
        else
        {
            PHX_Filesystem_OpenNode openNode;
            if ((result = filesystem->ops->createOpenNode(filesystem, node, &openNode)) != PHX_SUCCESS)
            {
                fputs("Could not create open node\n", stderr);
                return 1;
            }

            PHX_Filesystem_Entry entry;
            while ((result = filesystem->ops->dir_readEntry(filesystem, node, &openNode, &entry)) == PHX_SUCCESS)
            {
                if (!isSafeName(entry.name))
                {
                    fputs("Warning: Entry had a potentially malicious name: ", stderr);
                    printSafe(stderr, entry.name);
                    fputc('\n', stderr);
                    continue;
                }

                PHX_Filesystem_Node newNode;
                if ((result = filesystem->ops->getNode(filesystem, entry.node, &newNode)) != PHX_SUCCESS)
                {
                    fputs("Warning: Could not get node of entry\n", stderr);
                    continue;
                }

                const char* nameEnd = entry.name;
                while (*nameEnd) nameEnd++;
                const PHX_Size nameLen = (PHX_Size)(nameEnd - entry.name);

                char* newPath = filesystem->context->allocator->allocate(filesystem->context->allocator, hostPathLen + nameLen + 2);
                if (!newPath)
                {
                    result = PHX_ERROR_MEMORY;
                    filesystem->ops->cleanupNode(filesystem, &newNode);
                    break;
                }

                memcpy(newPath, hostPath, hostPathLen);
                newPath[hostPathLen] = '/';
                memcpy(newPath + hostPathLen + 1, entry.name, nameLen);
                newPath[hostPathLen + nameLen + 1] = '\0';

                if (extract(filesystem, &newNode, newPath, maxRecursionDepth, currentRecursionDepth + 1) != 0)
                {
                    filesystem->context->allocator->free(filesystem->context->allocator, newPath);
                    filesystem->ops->cleanupNode(filesystem, &newNode);
                    (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
                    return 1;
                }

                filesystem->context->allocator->free(filesystem->context->allocator, newPath);
                filesystem->ops->cleanupNode(filesystem, &newNode);
            }

            if (result != PHX_ERROR_NOT_FOUND)
            {
                fputs("Could not read all entries of directory\n", stderr);
                (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
                return 1;
            }

            (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
        }
    }
    else
    {
        fputs("Unknown type of entry\n", stderr);
        return 1;
    }

    return 0;
}

static int insert(PHX_Filesystem_Device* filesystem, PHX_Filesystem_Node* parentDir, const char* name, const char* hostPath, PHX_Size maxRecursionDepth, PHX_Size currentRecursionDepth)
{
    PHX_Result result;
    PHX_DetailedResult detailedResult;
    PHX_Filesystem_Node newNode;

    const char* hostPathEnd = hostPath;
    while (*hostPathEnd) hostPathEnd++;
    const PHX_Size hostPathLen = (PHX_Size)(hostPathEnd - hostPath);

    const char* queryPath = (hostPathLen == 0) ? "/" : hostPath;

    PHX_Native_Path_Type type;
    if (PHX_Native_Path_GetType(queryPath, &type) != PHX_TRUE)
    {
        fputs("Could not get type of path: ", stderr);
        printSafe(stderr, queryPath);
        fputc('\n', stderr);
        return 1;
    }

    // TODO: Attributes
    PHX_Filesystem_Node_Attribute attributes = 0;

    // TODO: Allow if types match, but clear file
    PHX_Filesystem_Entry tmpEntry;
    PHX_Filesystem_Node tmpNode;
    PHX_Bool exists = PHX_FALSE;
    result = (name[0] == '\0') ? PHX_SUCCESS : filesystem->ops->dir_lookupEntry(filesystem, parentDir, name, &tmpEntry);
    if (result == PHX_SUCCESS)
    {
        if (filesystem->ops->getNode(filesystem, (name[0] == '\0') ? filesystem->ops->rootNodeNumber : tmpEntry.node, &tmpNode) != PHX_SUCCESS)
        {
            fputs("Could not get node of entry\n", stderr);
            return 1;
        }
        exists = PHX_TRUE;
    }
    else if (result != PHX_ERROR_NOT_FOUND)
    {
        fputs("Could not look if entry already exists\n", stderr);
        return 1;
    }

    switch (type)
    {
        case PHX_NATIVE_TYPE_FILE:
        {
            if (exists == PHX_TRUE)
            {
                fprintf(stderr, "Entry for file %s already exists\n", hostPath);

                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            PHX_File_Device fileDevice;
            if ((detailedResult = PHX_Native_File_Open(queryPath, PHX_NATIVE_FILE_MODE_READ, &fileDevice, 0)).code != PHX_SUCCESS)
            {
                fputs("Could not open host file ", stderr);
                printSafe(stderr, queryPath);
                fprintf(stderr, ": %s\n", detailedResult.msg);

                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }
            const PHX_FileSize size = fileDevice.fileSize;

            if ((result = filesystem->ops->createNode(filesystem, parentDir, PHX_FILESYSTEM_NODE_TYPE_FILE, attributes, name, &newNode)) != PHX_SUCCESS)
            {
                fputs("Could not create file in filesystem: ", stderr);
                printSafe(stderr, name);
                fputc('\n', stderr);

                PHX_Native_File_Close(&fileDevice);
                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            PHX_Filesystem_OpenNode openNode;
            if ((result = filesystem->ops->createOpenNode(filesystem, &newNode, &openNode)) != PHX_SUCCESS)
            {
                fputs("Could not create open node\n", stderr);

                PHX_Native_File_Close(&fileDevice);
                filesystem->ops->cleanupNode(filesystem, &newNode);
                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            PHX_Byte buffer[4096];
            PHX_FileSize done = 0;
            while (done < size)
            {
                const PHX_FileSize remaining = size - done;
                const PHX_FileSize chunk = (remaining < sizeof(buffer)) ? remaining : sizeof(buffer);

                const PHX_FileSize read = PHX_Native_File_Read(&fileDevice, buffer, done, chunk);
                if (read == 0)
                    break;

                const PHX_FileSize written = (PHX_FileSize)filesystem->ops->file_write(filesystem, &newNode, &openNode, (PHX_FileSize)read, buffer);
                done += written;
                if (written != read)
                    break;
            }

            (void)filesystem->ops->closeOpenNode(filesystem, &openNode);
            PHX_Native_File_Close(&fileDevice);
            filesystem->ops->cleanupNode(filesystem, &newNode);

            if (done < size)
            {
                fputs("Error while reading host file and writing it to the filesystem\n", stderr);

                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            break;
        }

        case PHX_NATIVE_TYPE_DIRECTORY:
        {
            if (exists == PHX_TRUE && tmpNode.type != PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
            {
                fprintf(stderr, "Entry for directory %s already exists as a file\n", hostPath);

                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            const PHX_Bool intoParent = (name[0] == '\0') ? PHX_TRUE : PHX_FALSE;
            PHX_Filesystem_Node* dirNode = &newNode;

            if (exists == PHX_TRUE)
                dirNode = &tmpNode;
            else if (intoParent == PHX_TRUE)
                dirNode = parentDir;
            else if ((result = filesystem->ops->createNode(filesystem, parentDir, PHX_FILESYSTEM_NODE_TYPE_DIRECTORY, attributes, name, &newNode)) != PHX_SUCCESS)
            {
                fputs("Could not create directory in filesystem: ", stderr);
                printSafe(stderr, name);
                fputc('\n', stderr);

                if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                return 1;
            }

            if (maxRecursionDepth != 0 && currentRecursionDepth >= maxRecursionDepth)
            {
                fputs("Reached max recursion depth\n", stderr);
            }
            else
            {
                PHX_Native_Directory dir;
                if (PHX_Native_Directory_Open(queryPath, &dir) != PHX_TRUE)
                {
                    fputs("Could not open host directory ", stderr);
                    printSafe(stderr, queryPath);
                    fputc('\n', stderr);

                    if (intoParent != PHX_TRUE && exists != PHX_TRUE) filesystem->ops->cleanupNode(filesystem, &newNode);
                    if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                    return 1;
                }

                PHX_Native_Directory_Entry entry;
                while (PHX_Native_Directory_GetEntry(&dir, &entry) == PHX_TRUE)
                {
                    const PHX_Size nameLen = (PHX_Size)strlen(entry.name);

                    const PHX_Size sepLen = (hostPathLen == 0 || hostPath[hostPathLen - 1] != '/') ? 1 : 0;

                    char* childPath = filesystem->context->allocator->allocate(filesystem->context->allocator, hostPathLen + sepLen + nameLen + 1);
                    if (!childPath)
                    {
                        fputs("Could not allocate memory for child path\n", stderr);

                        PHX_Native_Directory_CleanupEntry(&entry);
                        PHX_Native_Directory_Close(&dir);
                        if (intoParent != PHX_TRUE && exists != PHX_TRUE) filesystem->ops->cleanupNode(filesystem, &newNode);
                        if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                        return 1;
                    }

                    memcpy(childPath, hostPath, hostPathLen);
                    if (sepLen)
                        childPath[hostPathLen] = '/';
                    memcpy(childPath + hostPathLen + sepLen, entry.name, nameLen);
                    childPath[hostPathLen + sepLen + nameLen] = '\0';

                    const int failed = insert(filesystem, dirNode, entry.name, childPath, maxRecursionDepth, currentRecursionDepth + 1);

                    filesystem->context->allocator->free(filesystem->context->allocator, childPath);
                    PHX_Native_Directory_CleanupEntry(&entry);

                    if (failed)
                    {
                        PHX_Native_Directory_Close(&dir);
                        if (intoParent != PHX_TRUE && exists != PHX_TRUE) filesystem->ops->cleanupNode(filesystem, &newNode);
                        if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
                        return 1;
                    }
                }

                PHX_Native_Directory_Close(&dir);
            }

            if (intoParent != PHX_TRUE && exists != PHX_TRUE) filesystem->ops->cleanupNode(filesystem, &newNode);

            break;
        }
        
        default:
            fputs("Invalid host entry type: ", stderr);
            printSafe(stderr, hostPath);
            fputc('\n', stderr);

            if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);
            break;
    }

    if (exists) filesystem->ops->cleanupNode(filesystem, &tmpNode);

    return 0;
}

static int filesystem(PHX_Context* context, const char* executable, const char* commandStr, const int argCount, const char** args)
{
    PHX_Result result;
    PHX_DetailedResult detailedResult;

    // TODO: Check for every single command if the targeted file already exists

    int fixedArgCount;
    enum
    {
        PHX_COMMAND_FILESYSTEM_INFO,
        PHX_COMMAND_FILESYSTEM_FORMAT,

        PHX_COMMAND_FILESYSTEM_EXTRACT,
        PHX_COMMAND_FILESYSTEM_INSERT,

        PHX_COMMAND_FILESYSTEM_MKDIR,
        PHX_COMMAND_FILESYSTEM_TOUCH,

        PHX_COMMAND_FILESYSTEM_LIST,
        PHX_COMMAND_FILESYSTEM_TREE,
        PHX_COMMAND_FILESYSTEM_CAT,

        PHX_COMMAND_FILESYSTEM_WRITE,
        PHX_COMMAND_FILESYSTEM_READ,

        PHX_COMMAND_FILESYSTEM_REMOVE,
        PHX_COMMAND_FILESYSTEM_MOVE,

        PHX_COMMAND_FILESYSTEM_BOOTSECTOR
    } command;

    if (argCount == 0 && strcmp(commandStr, "list") == 0)
    {
        for (PHX_Size i = 0; i < PHX_Filesystem_InterfaceCount; i++)
        {
            PHX_Filesystem_Interface* interface =PHX_Filesystem_Interfaces[i];
            fprintf(stdout, "Interface %zu:\n  Name: %s\n  Type: %s\n", i + 1, interface->name, interface->type);
        }

        return 0;
    }

    if (strcmp(commandStr, "info") == 0)
    {
        fixedArgCount = 1;
        command = PHX_COMMAND_FILESYSTEM_INFO;
    }
    else if (strcmp(commandStr, "format") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_FORMAT;
    }
    else if (strcmp(commandStr, "extract") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_EXTRACT;
    }
    else if (strcmp(commandStr, "insert") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_FILESYSTEM_INSERT;
    }
    else if (strcmp(commandStr, "mkdir") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_MKDIR;
    }
    else if (strcmp(commandStr, "touch") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_TOUCH;
    }
    else if (strcmp(commandStr, "list") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_LIST;
    }
    else if (strcmp(commandStr, "tree") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_TREE;
    }
    else if (strcmp(commandStr, "cat") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_CAT;
    }
    else if (strcmp(commandStr, "write") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_FILESYSTEM_WRITE;
    }
    else if (strcmp(commandStr, "read") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_FILESYSTEM_READ;
    }
    else if (strcmp(commandStr, "remove") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_REMOVE;
    }
    else if (strcmp(commandStr, "move") == 0)
    {
        fixedArgCount = 3;
        command = PHX_COMMAND_FILESYSTEM_MOVE;
    }
    else if (strcmp(commandStr, "bootsector") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_FILESYSTEM_BOOTSECTOR;
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

    const char* deviceStr = args[0];

    PHX_File_Device inFileDevice;
    PHX_Disk_Device diskDevice;
    PHX_Disk_Device partitionDevice;
    PHX_Disk_Device* device = PHX_Device_FromStr(context, deviceStr, &inFileDevice, &diskDevice, &partitionDevice);
    if (!device) return 1;

    PHX_Filesystem_Device filesystem;
    if (command == PHX_COMMAND_FILESYSTEM_FORMAT)
    {
        const char* format = args[1];
        PHX_Filesystem_Interface* interface = PHX_Filesystem_FindInterfaceByType(format);
        if (!interface)
        {
            fprintf(stderr, "Could not find filesystem interface for format \"%s\"\n", format);
            
            device->close(device);
            if (device == &partitionDevice)
                diskDevice.close(&diskDevice);
            return 1;
        }

        if ((result = interface->formatFilesystem(context, device, &filesystem, PHX_NULL)) != PHX_SUCCESS)
        {
            fputs("Error while formatting filesystem\n", stderr);
            
            device->close(device);
            if (device == &partitionDevice)
                diskDevice.close(&diskDevice);
            return 1;
        }
    }
    else
    {
        if ((result = PHX_Filesystem_Open(context, device, &filesystem)) != PHX_SUCCESS)
        {
            if (result == PHX_ERROR_FORMAT)
                fputs("Unknown format of filesystem\n", stderr);
            else
                fputs("Error while opening filesystem\n", stderr);

            device->close(device);
            if (device == &partitionDevice)
                diskDevice.close(&diskDevice);
            return 1;
        }
    }

    int returnCode = 0;
    switch (command)
    {
        case PHX_COMMAND_FILESYSTEM_INFO:
            fprintf(stdout, "%s (%s):\n", deviceStr, filesystem.type);
            fprintf(stdout, "  Id: %" PRIu64 "\n", filesystem.id);
            fprintf(stdout, "  Case-sensitive: %s\n", (filesystem.caseSensitive == PHX_TRUE) ? "Yes" : "No");
            break;


        case PHX_COMMAND_FILESYSTEM_FORMAT:
            break;
        

        case PHX_COMMAND_FILESYSTEM_EXTRACT:
        {
            const char* path = args[1];
            const char* hostPath = args[2];

            PHX_Filesystem_Node node;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &node)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find entry\n", stderr);
                else
                    fputs("Error while trying to find entry\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            returnCode = extract(&filesystem, &node, hostPath, 0, 0);
            filesystem.ops->cleanupNode(&filesystem, &node);
            break;
        }

        case PHX_COMMAND_FILESYSTEM_INSERT:
        {
            const char* path = args[1];
            const char* hostPath = args[2];

            if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
            {
                PHX_Filesystem_Node rootNode;
                if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &rootNode)) != PHX_SUCCESS)
                {
                    fputs("Could not get root directory\n", stderr);

                    returnCode = 1;
                    goto cleanup;
                }

                returnCode = insert(&filesystem, &rootNode, "", hostPath, 0, 0);
                filesystem.ops->cleanupNode(&filesystem, &rootNode);
                break;
            }

            char* parent;
            char name[PHX_NAME_LEN + 1];
            if ((result = PHX_Filesystem_SeparateParent(context, path, name, &parent)) != PHX_SUCCESS)
            {
                fputs("Error while separating parent\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            PHX_Filesystem_Node directoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, parent, PHX_NULL, &directoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find parent directory\n", stderr);
                else
                    fputs("Error while trying to find parent directory\n", stderr);

                returnCode = 1;
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }
            if (parent) context->allocator->free(context->allocator, parent);

            returnCode = insert(&filesystem, &directoryNode, name, hostPath, 0, 0);
            filesystem.ops->cleanupNode(&filesystem, &directoryNode);
            break;
        }


        case PHX_COMMAND_FILESYSTEM_MKDIR: case PHX_COMMAND_FILESYSTEM_TOUCH:
        {
            const char* path = args[1];
            if (strcmp(path, "") == 0 || strcmp(path, "/") == 0)
            {
                fputs("Cannot overwrite root\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            char* parent;
            char name[PHX_NAME_LEN + 1];
            if ((result = PHX_Filesystem_SeparateParent(context, path, name, &parent)) != PHX_SUCCESS)
            {
                fputs("Error while separating parent\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            PHX_Filesystem_Node directoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, parent, PHX_NULL, &directoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find parent directory\n", stderr);
                else
                    fputs("Error while trying to find parent directory\n", stderr);

                returnCode = 1;
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            PHX_Filesystem_Entry entry;
            if ((filesystem.ops->dir_lookupEntry(&filesystem, &directoryNode, name, &entry)) == PHX_SUCCESS)
            {
                fputs("Entry already exists\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            PHX_Filesystem_Node_Type type = (command == PHX_COMMAND_FILESYSTEM_MKDIR) ? PHX_FILESYSTEM_NODE_TYPE_DIRECTORY : PHX_FILESYSTEM_NODE_TYPE_FILE;

            if ((filesystem.ops->createNode(&filesystem, &directoryNode, type, 0, name, PHX_NULL)) != PHX_SUCCESS)
            {
                fputs("Could not create entry\n", stderr);
                returnCode = 1;
            }

            filesystem.ops->cleanupNode(&filesystem, &directoryNode);
            if (parent) context->allocator->free(context->allocator, parent);

            break;
        }


        case PHX_COMMAND_FILESYSTEM_LIST:
        {
            const char* path = args[1];
            PHX_Filesystem_Node directoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &directoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find directory\n", stderr);
                else
                    fputs("Error while trying to find directory\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            if (directoryNode.type != PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
            {
                fputs("Entry is not a directory\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                goto cleanup;
            }

            returnCode = printNode(&filesystem, &directoryNode, PHX_FALSE);
            filesystem.ops->cleanupNode(&filesystem, &directoryNode);
            break;
        }

        case PHX_COMMAND_FILESYSTEM_TREE:
        {
            const char* path = args[1];
            PHX_Filesystem_Node directoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &directoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find directory\n", stderr);
                else
                    fputs("Error while trying to find directory\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            if (directoryNode.type != PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
            {
                fputs("Entry is not a directory\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                goto cleanup;
            }

            fprintf(stdout, "%s:\n", path);
            returnCode = printNode(&filesystem, &directoryNode, PHX_TRUE);
            filesystem.ops->cleanupNode(&filesystem, &directoryNode);
            break;
        }

        case PHX_COMMAND_FILESYSTEM_CAT:
        {
            const char* path = args[1];
            PHX_Filesystem_Node node;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &node)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find file\n", stderr);
                else
                    fputs("Error while trying to find file\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            if (node.type != PHX_FILESYSTEM_NODE_TYPE_FILE)
            {
                fputs("Entry is not a file\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &node);
                goto cleanup;
            }

            PHX_Filesystem_OpenNode openNode;
            if ((result = filesystem.ops->createOpenNode(&filesystem, &node, &openNode)) != PHX_SUCCESS)
            {
                fputs("Could not create open node\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &node);
                goto cleanup;
            }

            PHX_Size read;
            char buffer[4096];
            while ((read = filesystem.ops->file_read(&filesystem, &node, &openNode, sizeof(buffer), buffer)) != 0)
            {
                fwrite(buffer, read, 1, stdout);
            }

            (void)filesystem.ops->closeOpenNode(&filesystem, &openNode);
            filesystem.ops->cleanupNode(&filesystem, &node);

            break;
        }

        case PHX_COMMAND_FILESYSTEM_READ: case PHX_COMMAND_FILESYSTEM_WRITE:
        {
            const char* path = args[1];
            PHX_Filesystem_Node node;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, path, PHX_NULL, &node)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fprintf(stderr, "Could not find file %s\n", path);
                else
                    fprintf(stderr, "Error while trying to find file %s\n", path);

                returnCode = 1;
                goto cleanup;
            }

            if (node.type != PHX_FILESYSTEM_NODE_TYPE_FILE)
            {
                fputs("Entry is not a file\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &node);
                goto cleanup;
            }

            const char* file = args[2];
            const PHX_Native_File_Mode fileMode = (command == PHX_COMMAND_FILESYSTEM_READ) ? PHX_NATIVE_FILE_MODE_CREATE : PHX_NATIVE_FILE_MODE_READ;
            const PHX_SectorSize fileSize = (command == PHX_COMMAND_FILESYSTEM_READ) ? node.size : 0;
            PHX_File_Device fileDevice;
            if ((detailedResult = PHX_Native_File_Open(file, fileMode, &fileDevice, fileSize)).code != PHX_SUCCESS)
            {
                fprintf(stderr, "Could not open file %s: %s\n", file, detailedResult.msg);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &node);
                goto cleanup;
            }

            PHX_Filesystem_OpenNode openNode;
            if ((result = filesystem.ops->createOpenNode(&filesystem, &node, &openNode)) != PHX_SUCCESS)
            {
                fputs("Could not create open node\n", stderr);

                returnCode = 1;
                PHX_Native_File_Close(&fileDevice);
                filesystem.ops->cleanupNode(&filesystem, &node);
                goto cleanup;
            }

            PHX_Size expected;
            PHX_Size read = 1;
            PHX_Size written = 0;
            char buffer[4096];
            if (command == PHX_COMMAND_FILESYSTEM_READ)
            {
                expected = node.size;
                while (read > 0 && written < node.size)
                {
                    read = filesystem.ops->file_read(&filesystem, &node, &openNode, sizeof(buffer), buffer);
                    written += PHX_Native_File_Write(&fileDevice, buffer, written, read);
                }
            }
            else // write
            {
                expected = fileDevice.fileSize;
                while (read > 0 && written < fileDevice.fileSize)
                {
                    read = PHX_Native_File_Read(&fileDevice, buffer, written, sizeof(buffer));
                    written += filesystem.ops->file_write(&filesystem, &node, &openNode, read, buffer);
                }
            }

            if (written < expected)
            {
                fputs("Could not read/write full content to file\n", stderr);
                returnCode = 1;
            }

            (void)filesystem.ops->closeOpenNode(&filesystem, &openNode);
            PHX_Native_File_Close(&fileDevice);
            filesystem.ops->cleanupNode(&filesystem, &node);

            break;
        }

        case PHX_COMMAND_FILESYSTEM_REMOVE:
        {
            const char* path = args[1];
            if (strcmp(path, "") == 0 || strcmp(path, "/") == 0)
            {
                fputs("Cannot remove root\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            char* parent;
            char name[PHX_NAME_LEN + 1];
            if ((result = PHX_Filesystem_SeparateParent(context, path, name, &parent)) != PHX_SUCCESS)
            {
                fputs("Error while separating parent\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            PHX_Filesystem_Node directoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, parent, PHX_NULL, &directoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find parent directory\n", stderr);
                else
                    fputs("Error while trying to find parent directory\n", stderr);

                returnCode = 1;
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            PHX_Filesystem_Entry entry;
            if ((result = filesystem.ops->dir_lookupEntry(&filesystem, &directoryNode, name, &entry)) != PHX_SUCCESS)
            {
                fputs("Could not find entry\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            PHX_Filesystem_Node node;
            if ((result = filesystem.ops->getNode(&filesystem, entry.node, &node)) != PHX_SUCCESS)
            {
                fputs("Error while trying to get node\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            if (node.type == PHX_FILESYSTEM_NODE_TYPE_DIRECTORY)
            {
                PHX_u64 entryCount;
                if ((result = filesystem.ops->dir_getEntryCount(&filesystem, &node, &entryCount)) != PHX_SUCCESS)
                {
                    fputs("Could not get count of children\n", stderr);

                    returnCode = 1;
                    filesystem.ops->cleanupNode(&filesystem, &node);
                    filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                    if (parent) context->allocator->free(context->allocator, parent);
                    goto cleanup;
                }

                if (entryCount != 0)
                {
                    fputs("Cannot remove a directory with children\n", stderr);

                    returnCode = 1;
                    filesystem.ops->cleanupNode(&filesystem, &node);
                    filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                    if (parent) context->allocator->free(context->allocator, parent);
                    goto cleanup;
                }
            }

            PHX_Filesystem_Node_Count newReferenceCount;
            if ((result = filesystem.ops->unlinkEntry(&filesystem, &directoryNode, name, &newReferenceCount)) != PHX_SUCCESS)
            {
                fputs("Error while trying to unlink entry\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &node);
                filesystem.ops->cleanupNode(&filesystem, &directoryNode);
                if (parent) context->allocator->free(context->allocator, parent);
                goto cleanup;
            }

            if (newReferenceCount == 0 && (result = filesystem.ops->removeNode(&filesystem, &node)) != PHX_SUCCESS)
            {
                fputs("Error while removing node\n", stderr);

                returnCode = 1;
            }

            filesystem.ops->cleanupNode(&filesystem, &node);
            filesystem.ops->cleanupNode(&filesystem, &directoryNode);
            if (parent) context->allocator->free(context->allocator, parent);

            break;
        }

        case PHX_COMMAND_FILESYSTEM_MOVE:
        {
            const char* srcPath = args[1];
            const char* dstPath = args[2];
            if (strcmp(srcPath, "") == 0 || strcmp(srcPath, "/") == 0 || strcmp(dstPath, "") == 0 || strcmp(dstPath, "/") == 0)
            {
                fputs("Cannot move root\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            char* srcParent;
            char srcName[PHX_NAME_LEN + 1];
            if ((result = PHX_Filesystem_SeparateParent(context, srcPath, srcName, &srcParent)) != PHX_SUCCESS)
            {
                fputs("Error while separating source parent\n", stderr);

                returnCode = 1;
                goto cleanup;
            }

            char* dstParent;
            char dstName[PHX_NAME_LEN + 1];
            if ((result = PHX_Filesystem_SeparateParent(context, dstPath, dstName, &dstParent)) != PHX_SUCCESS)
            {
                fputs("Error while separating destination parent\n", stderr);

                returnCode = 1;
                if (srcParent) context->allocator->free(context->allocator, srcParent);
                goto cleanup;
            }

            PHX_Filesystem_Node srcDirectoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, srcParent, PHX_NULL, &srcDirectoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find source parent directory\n", stderr);
                else
                    fputs("Error while trying to find source parent directory\n", stderr);

                returnCode = 1;
                if (srcParent) context->allocator->free(context->allocator, srcParent);
                if (dstParent) context->allocator->free(context->allocator, dstParent);
                goto cleanup;
            }

            PHX_Filesystem_Node dstDirectoryNode;
            if ((result = PHX_Filesystem_GetEntry(&filesystem, dstParent, PHX_NULL, &dstDirectoryNode)) != PHX_SUCCESS)
            {
                if (result == PHX_ERROR_NOT_FOUND)
                    fputs("Could not find destination parent directory\n", stderr);
                else
                    fputs("Error while trying to find destination parent directory\n", stderr);

                returnCode = 1;
                filesystem.ops->cleanupNode(&filesystem, &srcDirectoryNode);
                if (srcParent) context->allocator->free(context->allocator, srcParent);
                if (dstParent) context->allocator->free(context->allocator, dstParent);
                goto cleanup;
            }

            PHX_Filesystem_Node_Number newNodeNumber;
            if ((result = filesystem.ops->moveEntry(&filesystem, &srcDirectoryNode, srcName, &dstDirectoryNode, dstName, &newNodeNumber)) != PHX_SUCCESS)
            {
                fputs("Error while moving entry\n", stderr);

                returnCode = 1;
            }

            filesystem.ops->cleanupNode(&filesystem, &dstDirectoryNode);
            filesystem.ops->cleanupNode(&filesystem, &srcDirectoryNode);
            if (srcParent) context->allocator->free(context->allocator, srcParent);
            if (dstParent) context->allocator->free(context->allocator, dstParent);

            break;
        }

        case PHX_COMMAND_FILESYSTEM_BOOTSECTOR:
        {
            const char* bootsectorFileStr = args[1];

            PHX_File_Device bootsectorFileDevice;
            if ((detailedResult = PHX_Native_File_Open(bootsectorFileStr, PHX_NATIVE_FILE_MODE_READ, &bootsectorFileDevice, 0)).code != PHX_SUCCESS)
            {
                fprintf(stderr, "Could not open file %s: %s\n", bootsectorFileStr, detailedResult.msg);

                returnCode = 1;
                goto cleanup;
            }

            PHX_Byte bootsector[512];
            if (PHX_Native_File_Read(&bootsectorFileDevice, bootsector, 0, 512) != 512)
            {
                fputs("Error reading from bootsector file\n", stderr);

                returnCode = 1;
                PHX_Native_File_Close(&bootsectorFileDevice);
                goto cleanup;
            }

            if ((result = filesystem.ops->changeBootsector(&filesystem, bootsector)) != PHX_SUCCESS)
            {
                fputs("Error while changing bootsector of filesytem\n", stderr);
                returnCode = 1;
            }

            PHX_Native_File_Close(&bootsectorFileDevice);

            break;
        }
    }

cleanup:
    filesystem.ops->destroy(&filesystem);
    device->close(device);
    if (device == &partitionDevice)
        diskDevice.close(&diskDevice);

    return returnCode;
}

static int raw(PHX_Context* context, const char* executable, const char* commandStr, const int argCount, const char** args)
{
    PHX_DetailedResult detailedResult;

    int fixedArgCount;
    enum
    {
        PHX_COMMAND_RAW_READ,
        PHX_COMMAND_RAW_WRITE,
    } command;

    if (strcmp(commandStr, "read") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_RAW_READ;
    }
    else if (strcmp(commandStr, "write") == 0)
    {
        fixedArgCount = 2;
        command = PHX_COMMAND_RAW_WRITE;
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

    const char* deviceStr = args[0];

    PHX_File_Device inFileDevice;
    PHX_Disk_Device diskDevice;
    PHX_Disk_Device partitionDevice;
    PHX_Disk_Device* device = PHX_Device_FromStr(context, deviceStr, &inFileDevice, &diskDevice, &partitionDevice);
    if (!device) return 1;

    switch (command)
    {
        case PHX_COMMAND_RAW_READ: case PHX_COMMAND_RAW_WRITE:
        {
            const char* file = args[1];
            const PHX_Native_File_Mode fileMode = (command == PHX_COMMAND_RAW_READ) ? PHX_NATIVE_FILE_MODE_CREATE : PHX_NATIVE_FILE_MODE_READ;
            PHX_SectorSize fileSize = 0;
            if (command == PHX_COMMAND_RAW_READ)
            {
                // TODO: Overflow
                fileSize = device->sectorSize * device->totalSectors;
            }


            PHX_File_Device fileDevice;
            if ((detailedResult = PHX_Native_File_Open(file, fileMode, &fileDevice, fileSize)).code != PHX_SUCCESS)
            {
                fprintf(stderr, "Could not open file %s: %s\n", file, detailedResult.msg);

                device->close(device);
                if (device == &partitionDevice)
                    diskDevice.close(&diskDevice);
                return 1;
            }

            const PHX_Size blocksPerRead = (device->sectorSize >= 16384) ? 1 : (16384 / device->sectorSize);
            PHX_Byte* buffer = context->allocator->allocate(context->allocator, device->sectorSize * blocksPerRead);
            if (!buffer)
            {
                fputs("Could not allocate buffer\n", stderr);

                PHX_Native_File_Close(&fileDevice);
                device->close(device);
                if (device == &partitionDevice)
                    diskDevice.close(&diskDevice);
                return 1;
            }

            PHX_SectorSize currentBlock = 0;
            while (currentBlock < device->totalSectors)
            {
                const PHX_SectorSize chunk = ((currentBlock + blocksPerRead) > device->totalSectors) ? (device->totalSectors - currentBlock) : (blocksPerRead);

                if (command == PHX_COMMAND_RAW_READ)
                {
                    if (device->read(device, buffer, currentBlock, chunk) != chunk)
                    {
                        fputs("Could not read chunk from device\n", stderr);

                        context->allocator->free(context->allocator, buffer);
                        PHX_Native_File_Close(&fileDevice);
                        device->close(device);
                        if (device == &partitionDevice)
                            diskDevice.close(&diskDevice);
                        return 1;
                    }
                    if (PHX_Native_File_Write(&fileDevice, buffer, currentBlock * device->sectorSize, chunk * device->sectorSize) != chunk * device->sectorSize)
                    {
                        fputs("Could not write chunk to file\n", stderr);

                        context->allocator->free(context->allocator, buffer);
                        PHX_Native_File_Close(&fileDevice);
                        device->close(device);
                        if (device == &partitionDevice)
                            diskDevice.close(&diskDevice);
                        return 1;
                    }
                }
                else // write
                {
                    if (PHX_Native_File_Read(&fileDevice, buffer, currentBlock * device->sectorSize, chunk * device->sectorSize) != chunk * device->sectorSize)
                    {
                        fputs("Could not read chunk from file\n", stderr);

                        context->allocator->free(context->allocator, buffer);
                        PHX_Native_File_Close(&fileDevice);
                        device->close(device);
                        if (device == &partitionDevice)
                            diskDevice.close(&diskDevice);
                        return 1;
                    }
                    if (device->write(device, buffer, currentBlock, chunk) != chunk)
                    {
                        printf("device->write(device, buffer, %" PRIu64 ", %" PRIu64 ")\n", currentBlock, chunk);
                        fputs("Could not write chunk to device\n", stderr);

                        context->allocator->free(context->allocator, buffer);
                        PHX_Native_File_Close(&fileDevice);
                        device->close(device);
                        if (device == &partitionDevice)
                            diskDevice.close(&diskDevice);
                        return 1;
                    }
                }

                currentBlock += chunk;
            }

            context->allocator->free(context->allocator, buffer);
            PHX_Native_File_Close(&fileDevice);
            break;
        }
    }

    device->close(device);
    if (device == &partitionDevice)
        diskDevice.close(&diskDevice);

    return 0;
}


static void print_help(const char* name)
{
    FILE* const stream = stderr;

    fprintf(stream, "Usage:\n  %s <area> <command> [...]\n  %s <direct command> [...]", name, name);

    fputs("\nMeanings:\n", stream);
    fputs("  Image                                  Path referencing a file with any disk image format\n", stream);
    fputs("  Device                                 Path to a disk image and optionally with a partition number (':' + partition index)\n", stream);
    fputs("  Format                                 Type specifier for the interface\n", stream);
    fputs("  Start/Size                             Size specifier in bytes\n", stream);

    fputs(
        "\nSize specifier:\n"
        "  You can use the power-suffixes 'k' (p=1), 'm' (p=2), 'g' (p=3) and 't' (p=4).\n"
        "  The default base is 1024, if you use 'd' after the power-suffix, the base\n"
        "  will be 1000. Write any decimal number with '.' as decimal point (for example\n"
        "  100, 0.5, 32, 83.29), then the power-suffix and then the base-suffix.\n"
        "  The value will be calculated by 'n * b^p' (n = entered number, p = power,\n"
        "  b = base) and will be rounded to the nearest integer.\n",
        stream
    );

    fputs("\nDirect commands:\n", stream);
    fputs("  > help/-h                            Print this message\n", stream);
    // TODO: fputs("\n  > version/-v                         Print version message\n", stream);

    fputs("\nArea \"disk\":\n", stream);
    fputs("  Commands:\n", stream);
    fputs("  > list                                 List supported interfaces\n", stream);
    fputs("  > create <image> <format> <size>       Create a new disk image\n", stream);
    fputs("  > info <image>                         Print information about the disk image\n", stream);


    fputs(
        "\nArea \"partition\":\n"
        "  Commands:\n"
        "  > list                                 List supported interfaces\n"
        "  > types <format>                       List supported types for format\n"
        "  > info <image>                         Print information about the partition table\n"
        "  > create <image> <format>              Create empty partition table\n"
        "  > add <image> <index> <start> <size> [--bootable] [--type <type>]\n"
        "  -> Add partition to partition table\n"
        "  > remove <image> <index>               Remove partition from partition table\n"
        "  > bootsector <image> <file>            Set bootsector of partition table\n",
        stderr
    );
    // TODO: "  > signature <image> <signature>      Set signature of partition table\n"


    fputs("\nArea \"filesystem\":\n", stream);
    fputs("  Commands:\n", stream);
    fputs("  > list                                 List supported interfaces\n", stream);
    fputs("  > info <image>                         Print information about the filesystem\n", stream);
    fputs("  > format <image> <format>              Format a device with a filesystem\n", stream);
    fputs("  > extract <image <path> <host-path>    Extract entry to host entry\n", stream);
    fputs("  > insert <image <path> <host-path>     Insert entry from host entry\n", stream);
    fputs("  > mkdir <image> <path>                 Create a new directory\n", stream);
    fputs("  > touch <image> <path>                 Create a new empty file\n", stream);
    fputs("  > list <image> <path>                  List entries of a directory\n", stream);
    fputs("  > tree <image> <path>                  List entries of a directory recursively\n", stream);
    fputs("  > cat <image> <path>                   Print content of a file\n", stream);
    fputs("  > write <image> <path> <host-file>     Write content from a host file to a device file\n", stream);
    fputs("  > read <image> <path> <host-file>      Read content from a device file to a host file\n", stream);
    fputs("  > remove <image> <path>                Remove an entry\n", stream);
    fputs("  > move <image> <src-path> <dst-path>   Move an entry\n", stream);
    fputs("  > bootsector <image> <file>            Set bootsector of filesystem\n", stream);


    fputs("\nArea \"raw\":\n", stream);
    fputs("  Commands:\n", stream);
    fputs("  > read <device> <file>                 Read from device to file\n", stream);
    fputs("  > write <device> <file>                Read from file to device\n", stream);
}


static void print_version(void)
{
    FILE* const stream = stdout;

    fputs("PHX version " VERSION " (https://github.com/JonathanMohr/PHX)\n", stream);
    fputs("Compiled on " __DATE__ "\n", stream);
}


static void printWarning(PHX_Warning warning)
{
    const char* f = "Unknown warning: ";
    switch (warning.code)
    {
        case PHX_WARNING_FORMAT: f = "Format warning: "; break;
        default: break;
    }

    fputs(f, stderr);
    fputs(warning.msg, stderr);
    fputc('\n', stderr);
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

    if (strcmp(area, "version") == 0 || strcmp(area, "-v") == 0)
    {
        print_version();
        return 0;
    }

    if (argc < 3)
    {
        print_help(executable);
        return 1;
    }

    const char* command = argv[2];

    PHX_Context context = {
        printWarning,

        (PHX_Time)time(NULL),
        
        (PHX_u32)time(NULL),

        PHX_TRUE,

        &PHX_Native_Allocator
    };

    if (strcmp(area, "disk") == 0)
        return disk(&context, executable, command, argc - 3, argv + 3);
    if (strcmp(area, "partition") == 0)
        return partition(&context, executable, command, argc - 3, argv + 3);
    if (strcmp(area, "filesystem") == 0)
        return filesystem(&context, executable, command, argc - 3, argv + 3);
    if (strcmp(area, "raw") == 0)
        return raw(&context, executable, command, argc - 3, argv + 3);
    
    print_help(executable);
    return 1;
}
