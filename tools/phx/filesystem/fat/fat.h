#ifndef PHX_FILESYSTEM_FAT_FAT_H
#define PHX_FILESYSTEM_FAT_FAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHX_FILESYSTEM_FAT_TYPE "FAT-FILESYSTEM"

#include <filesystem/filesystem.h>

#define PHX_FILESYSTEM_FAT_HEADER_BPS 11
#define PHX_FILESYSTEM_FAT_HEADER_SPC 13
#define PHX_FILESYSTEM_FAT_HEADER_RES 14

#define PHX_FILESYSTEM_FAT_HEADER_FAC 16
#define PHX_FILESYSTEM_FAT_HEADER_RDC 17
#define PHX_FILESYSTEM_FAT_HEADER_TOS 19

#define PHX_FILESYSTEM_FAT_HEADER_MED 21

#define PHX_FILESYSTEM_FAT_HEADER_FAS 22

#define PHX_FILESYSTEM_FAT_HEADER_SPT 24
#define PHX_FILESYSTEM_FAT_HEADER_NOH 26

#define PHX_FILESYSTEM_FAT_HEADER_HIS 28
#define PHX_FILESYSTEM_FAT_HEADER_LTS 32

#define PHX_FILESYSTEM_FAT1X_HEADER_DRN 36
#define PHX_FILESYSTEM_FAT1X_HEADER_BOS 38

#define PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART 39

#define PHX_FILESYSTEM_FAT32_HEADER_FAS32 36
#define PHX_FILESYSTEM_FAT32_HEADER_EXF 40
#define PHX_FILESYSTEM_FAT32_HEADER_FSV 42
#define PHX_FILESYSTEM_FAT32_HEADER_ROC 44
#define PHX_FILESYSTEM_FAT32_HEADER_FIS 48
#define PHX_FILESYSTEM_FAT32_HEADER_BBS 50

#define PHX_FILESYSTEM_FAT32_HEADER_DRN 64
#define PHX_FILESYSTEM_FAT32_HEADER_BOS 66

#define PHX_FILESYSTEM_FAT32_HEADER_EXTSTART 67


#define PHX_FILESYSTEM_FAT_FSINFO_LES 0
#define PHX_FILESYSTEM_FAT_FSINFO_STS 484
#define PHX_FILESYSTEM_FAT_FSINFO_FCC 488
#define PHX_FILESYSTEM_FAT_FSINFO_NFC 452
#define PHX_FILESYSTEM_FAT_FSINFO_TRS 508


/*

    bootsector:
        u8 jmp[3]
        header
        u8 code[448/420] // 448 if FAT12/FAT16, 420 if FAT32
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

typedef enum
{
    PHX_FILESYSTEM_FAT_12,
    PHX_FILESYSTEM_FAT_16,
    PHX_FILESYSTEM_FAT_32
} PHX_Filesystem_FAT_Version;

typedef struct PHX_Filesystem_FAT_Data
{
    PHX_BlockDevice* usedDevice;
    PHX_Byte* buffer;

    PHX_Filesystem_FAT_Version version;

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
            PHX_u32 rootDirCluster;
            PHX_u16 backupBootsector;
        } fat32;
        struct
        {
            PHX_u32 rootDirSector;
            PHX_u16 rootDirEntryCount;
        } fat12_16;
    } specific;

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
