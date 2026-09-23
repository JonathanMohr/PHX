#ifndef PHX_FILESYSTEM_FAT_FAT_H
#define PHX_FILESYSTEM_FAT_FAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHX_FILESYSTEM_FAT_TYPE "FAT-FILESYSTEM"

#include <filesystem/filesystem.h>

/*

    bootsector:
        u8 jmp[3]
        header
        u8 code[420]
        u8 signature[2]

    header:
        u8 oemIdentifier[8]
        
        u16 bytesPerSector
        u8 sectorsPerCluster
        u16 reservedSectors

        u8 fatCount
        u16 rootDirEntryCount
        u16 totalSectors

        u8 mediaDescriptor

        u16 fatSize

        u16 sectorsPerTrack
        u16 numberOfHeads

        u32 hiddenSectors
        u32 largeTotalSectors

        { // if FAT32
            u32 fatSize32
            u16 extFlags
            u16 fsVersion
            u32 rootCluster
            u16 fsInfoSector
            u16 backupBootsector
            u8 reserved[12]
        }

        u8 driveNumber

        u8 reserved
        u8 bootSignature

        { // if extended boot signature
            u32 volumeID
        }

        { // if extended boot signature or extended boot signature old
            u8 volumeLabel[11]
            u8 filesystemType[8]
        }

    fs_info:
        u32 leadSignature
        u8 reserved[480]
        u32 structSignature
        u32 freeClusterCount
        u32 nextFreeCluster
        u8 reserved[12]
        u32 trailSignature

*/

typedef struct PHX_Filesystem_FAT_Data
{
    PHX_BlockDevice* usedDevice;
    PHX_Byte* buffer;

    PHX_u32 freeClusterCount;
    PHX_u32 nextFreeCluster;

    PHX_u32 fatSector;
    PHX_u32 fatSize;

    PHX_u32 dataSector;
    PHX_u32 dataSize;

    PHX_u32 bytesPerCluster;
    PHX_u32 totalSectors;

    union
    {
        struct
        {
            PHX_u32 cluster;
        } fat32;
        struct
        {
            PHX_u32 sector;
            PHX_u16 entryCount;
        } fat12_16;
    } rootDir;

    PHX_u16 bytesPerSector;
    PHX_u16 sectorsPerCluster;
    PHX_u16 reservedSectors;

    PHX_u16 activeFat;

    PHX_Byte fatCount;
    PHX_Byte mediaDescriptor;

    PHX_Byte bootsector[512];
    PHX_Byte fsInfo[512];

    PHX_Bool useDevice;
} PHX_Filesystem_FAT_Data;

extern PHX_Filesystem_Interface PHX_Filesystem_FAT_Interface;

#ifdef __cplusplus
}
#endif

#endif
