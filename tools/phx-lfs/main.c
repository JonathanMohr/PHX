#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "fat/fat.h"
#include "disk/disk.h"
#include "filesystem/filesystem.h"
#include "mbr/mbr.h"
#include "version.h"
#include "args.h"
#include "partition/table.h"

static void print_help(const char* name, FILE* s)
{
    fprintf(s, "Usage: %s ( <fs>: <image>(:<part>) | <disk>: <image> )\n", name);
    fprintf(s, "> %s create <partition/disk> mbr | fat12|fat16|fat32 [--size B/K/M/G/T] [--boot <file>] [--root <path>] [--start B/K/M/G/T] [flags]\n", name);
    fprintf(s, "> %s format <partition/disk> mbr | fat12|fat16|fat32 [--boot <file>] [--root <path>] [flags]\n", name);
    fprintf(s, "> %s insert <fs> <host path> [--path <image path>] [flags]\n", name);
    fprintf(s, "> %s extract <fs> <image path> [--path <host path>] [flags]\n", name);
    fprintf(s, "> %s remove <fs> <image path> [flags]\n", name);
    fprintf(s, "> %s remove <disk> <part> [flags]\n", name);
    fprintf(s, "> %s info <fs/disk> [flags]\n", name);
    fprintf(s, "> %s list <fs> <image path> [flags]\n", name);
    fprintf(s, "> %s list <disk> [flags]\n", name);
    fprintf(s, "> %s write <partition/disk> <host path> [--size B/K/M/G/T] [--start B/K/M/G/T] [flags].\n", name);
    fprintf(s, "> %s read <partition/disk> <host path> [--size B/K/M/G/T] [--start B/K/M/G/T] [flags].\n", name);
    fputc('\n', s);
    fputs("Commands:\n", s);
    fputs("> create                   Create a new image, format it, optionally set boot sector and root\n", s);
    fputs("> format                   Format an existing image, optionally set boot sector and root\n", s);
    fputs("> insert <fs>              Insert a file or directory from the host into the image\n", s);
    fputs("> extract <fs>             Extract a file or directory from the image into the host\n", s);
    fputs("> remove <fs>              Remove a file or directory from the image\n", s);
    fputs("> remove <disk>            Remove a partition from the image\n", s);
    fputs("> info/list <disk>         List partitions of the image\n", s);
    fputs("> info <fs>                Print information about the FS of the image\n", s);
    fputs("> list <fs>                List files at a specific path in the image\n", s);
    fputs("> write <disk/partition>   Write a file raw to the image or partition.\n", s);
    fputs("> read <disk/partition>    Read a file raw to the image or partition.\n", s);
    fputc('\n', s);
    fputs("Flags:\n", s);
    fputs("> --no-lfn                   Disable long file names for FAT\n", s);
    fputs("> --read-only                Open image in read-only mode\n", s);
    fputs("> --force-bootsector         Force writing bootsector without overriding header and signature\n", s);
    fputs("> --fast                     Prevent overriding image/partition\n", s);
    fputs("> --safe                     Prevent recursive deletion of directories\n", s);
    fputs("> --count-clusters           Count free clusters on FAT12 and FAT16 when using 'info'\n", s);
    fputs("> --dont-update-part-entry   Prevent updating of the partition entry\n", s);
    fputc('\n', s);
    fputs("Supported Partitions:\n", s);
    fputs("> MBR\n", s);
    fputs("Supported Filesystems:\n", s);
    fputs("> FAT12\n", s);
    fputs("> FAT16\n", s);
    fputs("> FAT32\n", s);
}

typedef unsigned char Subcommand;
#define COMMAND_NONE        ((Subcommand)0)
#define COMMAND_HELP        ((Subcommand)1)
#define COMMAND_VERSION     ((Subcommand)2)
#define COMMAND_CREATE      ((Subcommand)3)
#define COMMAND_FORMAT      ((Subcommand)4)
#define COMMAND_INSERT      ((Subcommand)5)
#define COMMAND_EXTRACT     ((Subcommand)6)
#define COMMAND_REMOVE      ((Subcommand)7)
#define COMMAND_INFO        ((Subcommand)8)
#define COMMAND_LIST        ((Subcommand)9)
#define COMMAND_WRITE       ((Subcommand)10)
#define COMMAND_READ        ((Subcommand)11)

int main(int argc, const char* argv[])
{
    fputs("Warning: phx-lfs is a direct lazy port\n", stderr);

    if (argc < 2) {
        print_help(argv[0], stderr);
        return 1;
    }

    Subcommand command = COMMAND_NONE;

    if      (ARG_IS_HELP(1))        command = COMMAND_HELP;
    else if (ARG_IS_VERSION(1))     command = COMMAND_VERSION;
    else if (ARG_CMP(1, "create"))  command = COMMAND_CREATE;
    else if (ARG_CMP(1, "format"))  command = COMMAND_FORMAT;
    else if (ARG_CMP(1, "insert"))  command = COMMAND_INSERT;
    else if (ARG_CMP(1, "extract")) command = COMMAND_EXTRACT;
    else if (ARG_CMP(1, "remove"))  command = COMMAND_REMOVE;
    else if (ARG_CMP(1, "info"))    command = COMMAND_INFO;
    else if (ARG_CMP(1, "list"))    command = COMMAND_LIST;
    else if (ARG_CMP(1, "write"))    command = COMMAND_WRITE;
    else if (ARG_CMP(1, "read"))    command = COMMAND_READ;
    else {
        fprintf(stderr, "Unknown command '%s'\n", argv[1]);
        return 1;
    }

    const char* image_str = argv[2];
    char mode_str[4] = {0, '+', 'b', 0};

    switch (command) {
        case COMMAND_CREATE:
            mode_str[0] = 'w';
            break;

        case COMMAND_FORMAT: case COMMAND_INSERT: 
        case COMMAND_EXTRACT: case COMMAND_REMOVE:
        case COMMAND_LIST: case COMMAND_INFO:
        case COMMAND_WRITE: case COMMAND_READ:
            mode_str[0] = 'r';
            break;

        case COMMAND_HELP: case COMMAND_VERSION:
        default:
            image_str = NULL;
            mode_str[0] = 0;
            break;
    }

    const char* p_name = image_str ? strrchr(image_str, ':') : NULL;
    uint64_t partition_number = 0;
    if (p_name) {
        const char* p_num = p_name + 1;
        uint64_t image_len = (uint64_t)p_name - (uint64_t)image_str;

        char* image_name = (char*)malloc(image_len + 1);
        if (!image_name) {
            fputs("Couldn't allocate memory for name\n", stderr);
            return 1;
        }

        memcpy(image_name, image_str, image_len);
        image_name[image_len] = '\0';

        image_str = image_name;

        mode_str[0] = 'r';

        partition_number = (uint64_t)atoll(p_num);
    }

    if (mode_str[0]) {
        if (argc < 3) {
            print_help(argv[0], stderr);
            return 1;
        }
    }

    int arg_start = argc;
    switch (command)
    {
        case COMMAND_HELP:           // TODO
        case COMMAND_VERSION: break; // TODO
        case COMMAND_CREATE:
        case COMMAND_FORMAT:
        case COMMAND_INSERT:
        case COMMAND_EXTRACT:
        case COMMAND_REMOVE:  arg_start = 4; break;
        case COMMAND_INFO:    arg_start = 3; break;
        case COMMAND_LIST:
        case COMMAND_WRITE:
        case COMMAND_READ:    arg_start = 4; break;
        default: break;
    }

    Arguments args = {0};
    if (ParseArguments(argc, argv, arg_start, &args) != 0) {
        fputs("Couldn't parse arguments\n", stderr);
        return 1;
    }

    // Commands that don't use the image
    switch (command)
    {
        case COMMAND_HELP: {
            print_help(argv[0], stdout);
            return 0;
        }
        case COMMAND_VERSION: {
            printVersion();
            return 0;
        }
        default: break;
    }

    uint64_t image_size = 0;
    if (mode_str[0] == 'w')
    {
        image_size = args.size;
    }
    else image_size = Path_GetSize(image_str);

    FILE* image_file = fopen(image_str, mode_str);
    if (!image_file) {
        fputs("Couldn't open the image\n", stderr);
        return 1;
    }

    if (p_name) free((void*)image_str);

    { // Set size
        uint8_t b = 0;
        (void)fseek(image_file, (long)image_size - 1, SEEK_SET);
        size_t __attribute__((unused)) _ = fread(&b, 1, 1, image_file);
        (void)fseek(image_file, (long)image_size - 1, SEEK_SET);
        (void)fwrite(&b, 1, 1, image_file);
        (void)fseek(image_file, 0, SEEK_SET);
    }

    // TODO
    Disk* disk = Disk_CreateFromFile(image_file, image_size, DISKFORMAT_RAW);
    if (!disk) {
        fputs("Couldn't open disk\n", stderr);
        fclose(image_file);
        return 1;
    }

    int is_fs_image = 0;
    int is_none = 0;

    if (command != COMMAND_CREATE && command != COMMAND_FORMAT) {
        uint8_t bootsector[512];
        uint64_t read = Disk_Read(disk, bootsector, 0, sizeof(bootsector));
        if (read != sizeof(bootsector)) {
            fputs("Couldn't read bootsector of disk\n", stderr);
            Disk_Close(disk);
            return 1;
        }

        FAT_Bootsector* fat_bootsector = (FAT_Bootsector*)&bootsector;
        if (
            memcmp(fat_bootsector->fat12_fat16.header.filesystem_type, "FAT12", 5) == 0 ||
            memcmp(fat_bootsector->fat12_fat16.header.filesystem_type, "FAT16", 5) == 0 ||
            memcmp(fat_bootsector->fat32.header.filesystem_type, "FAT32", 5) == 0
        ) {
            is_fs_image = 1;
        }
    } else {
        if (argc < 4) {
            print_help(argv[0], stderr);
            Disk_Close(disk);
            return 1;
        }
        const char* name_o = argv[3];

        uint64_t name_len = strlen(name_o);
        char* name = (char*)malloc(name_len + 1);
        if (!name) {
            fputs("Couldn't allocate memory for name\n", stderr);
            Disk_Close(disk);
            return 1;
        }
        memcpy(name, name_o, name_len);
        name[name_len] = '\0';

        if (strcmp(name, "fat12") == 0 || strcmp(name, "fat16") == 0 || strcmp(name, "fat32") == 0)
            is_fs_image = 1;
        
        else if (strcmp(name, "mbr") == 0)
            is_fs_image = 0;

        else if (strcmp(name, "none") == 0)
            is_none = 1;

        else {
            fprintf(stderr, "Unknown FS: %s\n", name_o);
        }

        free(name);
    }

    int bootable = args.boot_file != NULL;
    Partition* partition = NULL;

    if (p_name) {
        // TODO
        PartitionTable* pt = PartitionTable_Open(disk, PARTITIONTABLE_MBR);
        if (!pt) {
            fputs("Couldn't open partition table\n", stderr);
            Disk_Close(disk);
            return 1;
        }
        // FIXME
        if (command != COMMAND_CREATE) {
            partition = PartitionTable_GetPartition(pt, partition_number - 1, args.flag_read_only);
        } else {
            Partition* tmp = PartitionTable_GetPartition(pt, partition_number - 1, args.flag_read_only);
            if (tmp) {
                fputs("Partition already exists\n", stderr);
                Partition_Close(tmp);
                PartitionTable_Close(pt);
                Disk_Close(disk);
                return 1;
            }

            uint8_t p_type = MBR_TYPE_UNDEFINED;

            uint64_t start_of_search = args.start ? args.start : 1024LL * 1024;

            if (start_of_search < 512) start_of_search = 512;

            if (!args.size) {
                uint64_t free_start = PartitionTable_GetEndOfUsedRegion(pt, start_of_search);
                uint64_t free_end = disk->size;
                args.size = free_end - free_start;
            }

            if (args.size % disk->sectorSize != 0)
            {
                uint64_t remainder = args.size % disk->sectorSize;
                args.size += disk->sectorSize - remainder;
                fprintf(stderr, "Warning: Size not multiple of sector size, rounding up to %" PRIu64 "\n", args.size);
            }

            uint64_t start = PartitionTable_GetNextFreeRegion(pt, start_of_search, args.size);
            
            if (start + args.size > disk->size) {
                fputs("Not enough space\n", stderr);
                Partition_Close(partition);
                PartitionTable_Close(pt);
                Disk_Close(disk);
                return 1;
            }

            int result = PartitionTable_SetPartition(pt, partition_number - 1, start, args.size, p_type, bootable);
            if (result != 0) {
                fputs("Couldn't create partition\n", stderr);
                Partition_Close(partition);
                PartitionTable_Close(pt);
                Disk_Close(disk);
                return 1;
            }

            partition = PartitionTable_GetPartition(pt, partition_number - 1, args.flag_read_only);
        }

        PartitionTable_Close(pt);
        
        if (!is_none) is_fs_image = 1;
    } else {
        partition = Partition_Create(disk, 0, disk->size, args.flag_read_only);
    }

    if (command == COMMAND_WRITE || command == COMMAND_READ)
    {
        int result = 0;

        switch (command)
        {
            case COMMAND_WRITE: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }

                const char* host_path = argv[3];

                uint64_t size;
                if (args.size != 0) size = args.size;
                else                size = Path_GetSize(host_path);

                FILE* host_file = fopen(host_path, "rb");
                if (!host_file) {
                    fprintf(stderr, "Couldn't open %s\n", host_path);
                    result = 1;
                }

                // TODO
                uint8_t buffer[4096];
                uint64_t offset = args.start;
                size += args.start;

                while (offset < size)
                {
                    uint64_t to_write = sizeof(buffer);
                    if (offset + to_write > size)
                        to_write = size - offset;

                    uint64_t read_bytes = fread(buffer, 1, to_write, host_file);
                    if (read_bytes != to_write) {
                        fprintf(stderr, "Warning: Couldn't read %" PRIu64 " bytes of %s\n", size, host_path);
                        break;
                    }

                    uint64_t written = Partition_Write(partition, buffer, offset, read_bytes);
                    if (written != read_bytes) {
                        fprintf(stderr, "Error: Couldn't write %" PRIu64 " bytes to partition\n", size);
                        break;
                    }

                    offset += written;
                }

                fclose(host_file);
                break;
            }
            case COMMAND_READ: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }

                const char* host_path = argv[3];

                uint64_t size;
                if (args.size != 0) size = args.size;
                else                size = partition->size;

                FILE* host_file = fopen(host_path, "w+b");
                if (!host_file) {
                    fprintf(stderr, "Couldn't open %s\n", host_path);
                    result = 1;
                }

                // TODO
                uint8_t buffer[4096];
                uint64_t offset = args.start;
                size += args.start;

                while (offset < size)
                {
                    uint64_t to_read = sizeof(buffer);
                    if (offset + to_read > size)
                        to_read = size - offset;

                    uint64_t read_bytes = Partition_Read(partition, buffer, offset, to_read);
                    if (read_bytes != to_read) {
                        fprintf(stderr, "Warning: Couldn't read %" PRIu64 " bytes of partition\n", size);
                        break;
                    }

                    uint64_t written = fwrite(buffer, 1, read_bytes, host_file);
                    if (written != read_bytes) {
                        fprintf(stderr, "Warning: Couldn't write %" PRIu64 " bytes to %s\n", size, host_path);
                    }

                    offset += written;
                }

                fclose(host_file);
                break;
            }
            default: break;
        }

        Partition_Close(partition);
        Disk_Close(disk);
        return result;
    }

    if (is_none) {
        if (!partition) {
            fputs("Couldn't open partition\n", stderr);
            Disk_Close(disk);
            return 1;
        }

        uint8_t zero_block[CHUNK_SIZE] = {0};
        uint64_t offset = 0;

        while (offset < partition->size && !args.flag_fast) {
            uint64_t chunk = (partition->size - offset) < CHUNK_SIZE ? (partition->size - offset) : CHUNK_SIZE;
            if (Partition_Write(partition, (uint8_t*)zero_block, offset, chunk) != chunk) {
                fputs("Error while writing to partition", stderr);
                Partition_Close(partition);
                Disk_Close(disk);
                return 1;
            }
            offset += chunk;
        }

        return 0;
    }
    else if (is_fs_image) {
        if (!partition) {
            fputs("Couldn't open partition\n", stderr);
            Disk_Close(disk);
            return 1;
        }

        Filesystem_Type fs_type = FILESYSTEM_FAT12; // TODO
        if (command != COMMAND_CREATE && command != COMMAND_FORMAT) {
            FAT_Bootsector bootsector;
            if (Partition_Read(partition, &bootsector, 0, sizeof(FAT_Bootsector)) != sizeof(FAT_Bootsector)) {
                fputs("Couldn't read bootsector of file\n", stderr);
                Partition_Close(partition);
                Disk_Close(disk);
                return 1;
            }

            if (memcmp(bootsector.fat12_fat16.header.filesystem_type, "FAT12", 5) == 0) {
                fs_type = FILESYSTEM_FAT12;
            } else if (memcmp(bootsector.fat12_fat16.header.filesystem_type, "FAT16", 5) == 0) {
                fs_type = FILESYSTEM_FAT16;
            } else if (memcmp(bootsector.fat32.header.filesystem_type, "FAT32", 5) == 0) {
                fs_type = FILESYSTEM_FAT32;
            }
        } else {
            if (argc < 4) {
                print_help(argv[0], stderr);
                Partition_Close(partition);
                Disk_Close(disk);
                return 1;
            }
            const char* fs_name = argv[3];
            if (
                (fs_name[0] == 'f' || fs_name[0] == 'F') &&
                (fs_name[1] == 'a' || fs_name[1] == 'A') &&
                (fs_name[2] == 't' || fs_name[2] == 'T')
            ) {
                //FAT
                if      (fs_name[3] == '1' && fs_name[4] == '2') fs_type = FILESYSTEM_FAT12;
                else if (fs_name[3] == '1' && fs_name[4] == '6') fs_type = FILESYSTEM_FAT16;
                else if (fs_name[3] == '3' && fs_name[4] == '2') fs_type = FILESYSTEM_FAT32;
                else fprintf(stderr, "Unknown FS: %s\n", fs_name);
            } else {
                fprintf(stderr, "Unknown FS: %s\n", fs_name);
            }
        }

        Fat_Version fat_version;
        switch (fs_type) {
            case FILESYSTEM_FAT12: fat_version = FAT_VERSION_12; break;
            case FILESYSTEM_FAT16: fat_version = FAT_VERSION_16; break;
            case FILESYSTEM_FAT32: fat_version = FAT_VERSION_32; break;
            default:
                Partition_Close(partition);
                Disk_Close(disk);
                fputs("Unknown FS: internal\n", stderr);
                return 1;
        }

        uint8_t bootsector_buffer[512];
        if (args.boot_file) {
            FILE* bootcode_f = fopen(args.boot_file, "rb");
            if (!bootcode_f) {
                Partition_Close(partition);
                Disk_Close(disk);
                fprintf(stderr, "Couldn't open '%s'\n", args.boot_file);
                return 1;
            }
            if (fread(bootsector_buffer, sizeof(bootsector_buffer), 1, bootcode_f) != 1) {
                Partition_Close(partition);
                Disk_Close(disk);
                fprintf(stderr, "Couldn't read 512 bytes of '%s'\n", args.boot_file);
                return 1;
            }
            fclose(bootcode_f);
        }

        FAT_Filesystem* fat_fs = NULL;
        if (command == COMMAND_CREATE || command == COMMAND_FORMAT) {
            const char* oem_name = "lfs";
            const char* volume_name = "NO NAME";
            uint32_t volume_id = 0x12345678;

            uint32_t bytes_per_sector = (uint32_t)disk->sectorSize;
            uint8_t sectors_per_cluster = fat_version == FAT_VERSION_16 ? 4 : 1;
            uint16_t reserved_sectors = 32;
            switch (fat_version) {
                case FAT_VERSION_12: reserved_sectors = 1; break;
                case FAT_VERSION_16: reserved_sectors = 4; break;
                case FAT_VERSION_32: reserved_sectors = 32; break;
                default: break;
            }
            uint8_t number_of_fats = 2;
            uint16_t max_root_directory_entries = 0;
            switch (fat_version) {
                case FAT_VERSION_12: max_root_directory_entries = 224; break;
                case FAT_VERSION_16: max_root_directory_entries = 512; break;
                case FAT_VERSION_32: max_root_directory_entries = 0; break;
                default: break;
            }
            uint16_t sectors_per_track = fat_version == FAT_VERSION_12 ? 18 : 32;
            uint16_t number_of_heads = fat_version == FAT_VERSION_32 ? 8 : 2;
            uint8_t drive_number = fat_version == FAT_VERSION_12 ? 0x00 : 0x80;
            uint8_t media_descriptor = fat_version == FAT_VERSION_12 ? FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_FLOPPY144 : FAT_BOOTSECTOR_MEDIA_DESCRIPTOR_DISK;

            uint32_t hidden_sectors = (uint32_t)(partition->offset / bytes_per_sector);

            // TODO
            fat_fs = FAT_CreateEmptyFilesystem(partition, fat_version, args.flag_fast, (args.boot_file ? bootsector_buffer : NULL), args.flag_force_bootsector,
                                            oem_name, volume_name, volume_id,
                                            partition->size, bytes_per_sector,
                                            sectors_per_cluster, reserved_sectors,
                                            number_of_fats, max_root_directory_entries,
                                            sectors_per_track, number_of_heads,
                                            drive_number, media_descriptor, hidden_sectors);

            if (p_name && !args.flag_dont_update_partition_entry) {
                // TODO
                PartitionTable* pt = PartitionTable_Open(disk, PARTITIONTABLE_MBR);

                uint8_t p_type = MBR_TYPE_FAT12; // TODO
                switch (fs_type) { // TODO
                    case FILESYSTEM_FAT12: p_type = MBR_TYPE_FAT12; break;
                    case FILESYSTEM_FAT16: p_type = MBR_TYPE_FAT16_LBA; break;
                    case FILESYSTEM_FAT32: p_type = MBR_TYPE_FAT32_LBA; break;
                    default: break;
                }

                int result = PartitionTable_SetPartition(pt, partition_number - 1, partition->offset, partition->size, p_type, bootable);
                if (result != 0) {
                    fputs("Warning: Couldn't update partition entry\n", stderr);
                }

                PartitionTable_Close(pt);
            }
        } else {
            fat_fs = FAT_OpenFilesystem(partition, fat_version, args.flag_read_only);
        }

        if (!fat_fs) {
            fputs("Couldn't open FAT FS\n", stderr);
            Partition_Close(partition);
            Disk_Close(disk);
            return 1;
        }

        int fat_use_lfn = !args.flag_no_lfn;
        Filesystem* fs = Filesystem_CreateFromFAT(fat_fs, fat_use_lfn);
        if (!fs) {
            fputs("Couldn't open FS\n", stderr);
            FAT_CloseFilesystem(fat_fs);
            Partition_Close(partition);
            Disk_Close(disk);
            return 1;
        }

        int result = 0;

        switch (command)
        {
            case COMMAND_CREATE: case COMMAND_FORMAT: {
                if (args.root_path) {
                    if (Filesystem_SyncPathsToFS(fs->root, "/", args.root_path) != 0) {
                        fprintf(stderr, "Couldn't sync '%s' to the root\n", args.root_path);
                        result = 1;
                    }
                }
                break;
            }
            case COMMAND_INSERT: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }
                const char* host_path = argv[3];
                const char* image_path = args.path ? args.path : argv[3];
                if (Filesystem_SyncPathsToFS(fs->root, image_path, host_path) != 0) {
                    fprintf(stderr, "Couldn't sync '%s' to '%s'\n", host_path, image_path);
                    result = 1;
                }
                break;
            }
            case COMMAND_EXTRACT: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }
                const char* host_path = args.path ? args.path : argv[3];
                const char* image_path = argv[3];
                if (Filesystem_SyncPathsFromFS(fs->root, image_path, host_path) != 0) {
                    fprintf(stderr, "Couldn't sync '%s' to '%s'\n", image_path, host_path);
                    result = 1;
                }
                break;
            }
            case COMMAND_REMOVE: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }
                const char* image_path = argv[3];
                int operation_result = Filesystem_DeletePath(fs->root, image_path, args.flag_safe);
                if (operation_result == 2) {
                    fprintf(stderr, "Directory '%s' isn't empty\n", image_path);
                    result = 1;
                } else if (operation_result != 0) {
                    fprintf(stderr, "Couldn't remove '%s'\n", image_path);
                    result = 1;
                }
                break;
            }
            case COMMAND_INFO: {
                FAT_DumpInfo(fat_fs, stdout, args.flag_count_clusters);
                break;
            }
            case COMMAND_LIST: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    Partition_Close(partition);
                    Disk_Close(disk);
                    return 1;
                }
                const char* image_path = argv[3];
                Filesystem_File* image_dir = Filesystem_OpenPath(fs->root, image_path, 0, 0, 0, 0, 0, 0, 0, 0);
                if (!image_dir) {
                    fprintf(stderr, "Couldn't open '%s'\n", image_path);
                    result = 1;
                    break;
                }
                if (Filesystem_PrintAll(image_dir, image_path, 0) != 0) {
                    fprintf(stderr, "Couldn't list '%s'\n", image_path);
                    result = 1;
                }
                Filesystem_CloseEntry(image_dir);
                break;
            }
            default: break;
        }

        Filesystem_Close(fs);
        Partition_Close(partition);
        Disk_Close(disk);
        return result;
    } else {
        uint8_t bootsector_buffer[512];
        if (args.boot_file) {
            FILE* bootcode_f = fopen(args.boot_file, "rb");
            if (!bootcode_f) {
                fprintf(stderr, "Couldn't open '%s'\n", args.boot_file);
                Disk_Close(disk);
                return 1;
            }
            if (fread(bootsector_buffer, sizeof(bootsector_buffer), 1, bootcode_f) != 1) {
                fprintf(stderr, "Couldn't read first 512 bytes of '%s'\n", args.boot_file);
                Disk_Close(disk);
                return 1;
            }
            fclose(bootcode_f);
        }

        PartitionTable* pt = NULL;
        if (command != COMMAND_CREATE && command != COMMAND_FORMAT) {
            pt = PartitionTable_Open(disk, PARTITIONTABLE_MBR); // TODO
        } else {
            if (disk->size < 512) {
                fputs("Disk needs at least 512 byte to even get it's initial structure\n", stderr);
                Disk_Close(disk);
                return 1;
            } // TODO
            pt = PartitionTable_Create(disk, PARTITIONTABLE_MBR, args.flag_fast, (args.boot_file ? bootsector_buffer : NULL), args.flag_force_bootsector);
        }
        if (!pt) {
            fputs("Couldn't open partition table\n", stderr);
            Disk_Close(disk);
            return 1;
        }

        switch (command)
        {
            case COMMAND_CREATE: case COMMAND_FORMAT: {
                break;
            }

            case COMMAND_REMOVE: {
                if (argc < 4) {
                    print_help(argv[0], stderr);
                    PartitionTable_Close(pt);
                    Disk_Close(disk);
                    return 1;
                }
                const char* num_str = argv[3];

                uint64_t p_num = (uint64_t)strtoull(num_str, NULL, 10);
                if (p_num == 0 || p_num > 4) {
                    fprintf(stderr, "Invalid partition %" PRIu64 "\n", p_num);
                    PartitionTable_Close(pt);
                    Disk_Close(disk);
                    return 1;
                }

                if (!args.flag_fast) {
                    int result = PartitionTable_ClearPartition(pt, (uint8_t)(p_num - 1));
                    if (result != 0) {
                        // TODO
                    }
                }

                int result = PartitionTable_DeletePartition(pt, (uint8_t)(p_num - 1));
                if (result == 2) {
                    fprintf(stderr, "Partition %" PRIu64 " doesn't exist\n", p_num);
                } else if (result != 0) {
                    fprintf(stderr, "Couldn't delete partition %" PRIu64 "\n", p_num);
                    PartitionTable_Close(pt);
                    Disk_Close(disk);
                    return 1;
                }

                break;
            }

            case COMMAND_INFO: case COMMAND_LIST: {
                PartitionTable_PrintAll(pt, image_str);
                break;
            }

            case COMMAND_INSERT: {
                fputs("Can't use insert on a disk with partitions\n", stderr);
                break;
            }

            case COMMAND_EXTRACT: {
                fputs("Can't use extract on a disk with partitions\n", stderr);
                break;
            }

            default: break;
        }

        PartitionTable_Close(pt);
        Disk_Close(disk);
        return 0;
    }
}
