#ifndef PHX_FILESYSTEM_FAT_FAT_H
#define PHX_FILESYSTEM_FAT_FAT_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHX_FILESYSTEM_FAT_TYPE "FAT-FILESYSTEM"

#include <filesystem/filesystem.h>
#include <base.h>
#include <endianness.h>

#define PHX_FILESYSTEM_FAT_HEADER_OEM 3

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
#define PHX_FILESYSTEM_FAT1X_HEADER_RES 37
#define PHX_FILESYSTEM_FAT1X_HEADER_BOS 38

#define PHX_FILESYSTEM_FAT1X_HEADER_EXTSTART 39

#define PHX_FILESYSTEM_FAT32_HEADER_FAS32 36
#define PHX_FILESYSTEM_FAT32_HEADER_EXF 40
#define PHX_FILESYSTEM_FAT32_HEADER_FSV 42
#define PHX_FILESYSTEM_FAT32_HEADER_ROC 44
#define PHX_FILESYSTEM_FAT32_HEADER_FIS 48
#define PHX_FILESYSTEM_FAT32_HEADER_BBS 50
#define PHX_FILESYSTEM_FAT32_HEADER_RES12 52

#define PHX_FILESYSTEM_FAT32_HEADER_DRN 64
#define PHX_FILESYSTEM_FAT32_HEADER_RES 65
#define PHX_FILESYSTEM_FAT32_HEADER_BOS 66

#define PHX_FILESYSTEM_FAT32_HEADER_EXTSTART 67


#define PHX_FILESYSTEM_FAT_FSINFO_LES 0
#define PHX_FILESYSTEM_FAT_FSINFO_STS 484
#define PHX_FILESYSTEM_FAT_FSINFO_FCC 488
#define PHX_FILESYSTEM_FAT_FSINFO_NFC 452
#define PHX_FILESYSTEM_FAT_FSINFO_TRS 508


#define PHX_FILESYSTEM_FAT_BOOT_SIGNATURE_EXTENDED_BOOT_SIGNATURE_OLD 0x28
#define PHX_FILESYSTEM_FAT_BOOT_SIGNATURE_EXTENDED_BOOT_SIGNATURE 0x29


#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_DISK                  0xF8

#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_1_44M_OR_2_88M 0xF0
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_720K_OR_1_2M   0xF9
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_320K_1P        0xFA
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_640K           0xFB
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_180K           0xFC
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_360K           0xFD
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_160K           0xFE
#define PHX_FILESYSTEM_FAT_MEDIA_DESCRIPTOR_FLOPPY_320K_2P        0xFF


#define PHX_FILESYSTEM_FAT_FSINFO_LEAD_SIGNATURE   0x41615252
#define PHX_FILESYSTEM_FAT_FSINFO_STRUCT_SIGNATURE 0x61417272
#define PHX_FILESYSTEM_FAT_FSINFO_TRAIL_SIGNATURE  0xAA550000


#define PHX_FILESYSTEM_FAT_DIRENT_NAM 0
#define PHX_FILESYSTEM_FAT_DIRENT_EXT 8
#define PHX_FILESYSTEM_FAT_DIRENT_ATR 11
#define PHX_FILESYSTEM_FAT_DIRENT_RES 12
#define PHX_FILESYSTEM_FAT_DIRENT_CTT 13
#define PHX_FILESYSTEM_FAT_DIRENT_CRT 14
#define PHX_FILESYSTEM_FAT_DIRENT_CRD 16
#define PHX_FILESYSTEM_FAT_DIRENT_LAD 18
#define PHX_FILESYSTEM_FAT_DIRENT_FCH 20
#define PHX_FILESYSTEM_FAT_DIRENT_LMT 22
#define PHX_FILESYSTEM_FAT_DIRENT_LMD 24
#define PHX_FILESYSTEM_FAT_DIRENT_FCL 26
#define PHX_FILESYSTEM_FAT_DIRENT_FIS 28

#define PHX_FILESYSTEM_FAT_LFNENT_ORD 0
#define PHX_FILESYSTEM_FAT_LFNENT_NA1 1
#define PHX_FILESYSTEM_FAT_LFNENT_ATR 11
#define PHX_FILESYSTEM_FAT_LFNENT_RES 12
#define PHX_FILESYSTEM_FAT_LFNENT_CHE 13
#define PHX_FILESYSTEM_FAT_LFNENT_NA2 14
#define PHX_FILESYSTEM_FAT_LFNENT_RES16 26
#define PHX_FILESYSTEM_FAT_LFNENT_NA3 28

#define PHX_FILESYSTEM_FAT_DIRENT_SIZE 32

#define PHX_FILESYSTEM_FAT_ENTRY_FREE         0x00
#define PHX_FILESYSTEM_FAT_ENTRY_DELETED      0xE5
#define PHX_FILESYSTEM_FAT_ENTRY_KANJI_ESCAPE 0x05

#define PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE 0x0F

#define PHX_FILESYSTEM_FAT_ENTRY_READONLY     0x01
#define PHX_FILESYSTEM_FAT_ENTRY_HIDDEN       0x02
#define PHX_FILESYSTEM_FAT_ENTRY_SYSTEM       0x04
#define PHX_FILESYSTEM_FAT_ENTRY_VOLUME_LABEL 0x08
#define PHX_FILESYSTEM_FAT_ENTRY_DIRECTORY    0x10
#define PHX_FILESYSTEM_FAT_ENTRY_ARCHIVE      0x20


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


    directory_entry:
        u8 name[8]
        u8 ext[3]

        u8 attribute

        u8 reserved

        u8 creationTimeTenths
        u16 creationTime
        u16 creationDate

        u16 lastAccessDate

        u16 firstClusterHigh

        u16 lastModificationTime
        u16 lastModificationDate

        u16 firstCluster

        u32 fileSize

    lfn_entry:
        u8 order
        
        u16 name1[5]

        u8 attribute
        u8 reserved
        u8 checksum

        u16 name2[6]

        u16 reserved

        u16 name3[2]

*/

#define PHX_Filesystem_FAT_Read_u8 read_u8
#define PHX_Filesystem_FAT_Read_u16 read_u16
#define PHX_Filesystem_FAT_Read_u32 read_u32
#define PHX_Filesystem_FAT_Write_u8 write_u8
#define PHX_Filesystem_FAT_Write_u16 write_u16
#define PHX_Filesystem_FAT_Write_u32 write_u32

static inline PHX_Byte read_u8(const PHX_Byte* buffer)
{
    PHX_Byte val;
    memcpy(&val, buffer, sizeof(val));
    return val;
}

static inline PHX_u16 read_u16(const PHX_Byte* buffer)
{
    PHX_u16 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u16_Le(val);
}

static inline PHX_u32 read_u32(const PHX_Byte* buffer)
{
    PHX_u32 val;
    memcpy(&val, buffer, sizeof(val));
    return Endian_Convert_u32_Le(val);
}

static inline void write_u8(PHX_Byte* buffer, PHX_Byte val)
{
    memcpy(buffer, &val, sizeof(val));
}

static inline void write_u16(PHX_Byte* buffer, PHX_u16 val)
{
    PHX_u16 rawVal = Endian_Convert_u16_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}

static inline void write_u32(PHX_Byte* buffer, PHX_u32 val)
{
    PHX_u32 rawVal = Endian_Convert_u32_Le(val);
    memcpy(buffer, &rawVal, sizeof(rawVal));
}

typedef enum
{
    PHX_FILESYSTEM_FAT_12,
    PHX_FILESYSTEM_FAT_16,
    PHX_FILESYSTEM_FAT_32
} PHX_Filesystem_FAT_Version;

#define PHX_FILESYSTEM_FAT_ACTIVE_ALL 0xFFFF

typedef struct PHX_Filesystem_FAT_Data
{
    PHX_BlockDevice* usedDevice;
    PHX_Byte* buffer;
    PHX_Byte* clusterBuffer;

    PHX_Filesystem_FAT_Version version;

    PHX_u32 freeClusterCount;
    PHX_u32 nextFreeCluster;

    PHX_u32 fatSector;
    PHX_u32 fatSize;

    PHX_u32 dataSector;
    PHX_u32 dataSize;

    PHX_u32 bytesPerCluster;
    PHX_u32 totalSectors;
    PHX_u32 totalClusters;

    union
    {
        struct
        {
            PHX_u32 rootDirCluster;
            PHX_u16 fsInfoSector;
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

    PHX_Bool writeWithLFN;
    PHX_Bool readWithLFN;

    PHX_Bool useDevice;
} PHX_Filesystem_FAT_Data;


typedef struct
{
    PHX_u32 startCluster;
} PHX_Filesystem_FAT_Node_Extra;

typedef struct
{
    PHX_u32 currentCluster;
} PHX_Filesystem_FAT_OpenNode_Extra;


extern PHX_Filesystem_Interface PHX_Filesystem_FAT_Interface;

PHX_Bool PHX_Filesystem_FAT_WriteBootsector(PHX_Filesystem_FAT_Data* data);

PHX_Bool PHX_Filesystem_FAT_ReadFsInfo(PHX_Filesystem_FAT_Data* data);
PHX_Bool PHX_Filesystem_FAT_WriteFsInfo(PHX_Filesystem_FAT_Data* data);

void PHX_Filesystem_FAT_UpdateFsInfo(PHX_Filesystem_FAT_Data* data);

#define PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT 0xFFFFFFFFFFFFFFFF

#define PHX_FILESYSTEM_FAT_CLUSTER_VALUE_EOC 0xFFFFFFFF

#define PHX_FILESYSTEM_FAT_CLUSTER_NORMAL 0
#define PHX_FILESYSTEM_FAT_CLUSTER_FREE   1
#define PHX_FILESYSTEM_FAT_CLUSTER_BAD    2
#define PHX_FILESYSTEM_FAT_CLUSTER_EOC    3
#define PHX_FILESYSTEM_FAT_CLUSTER_ERROR  4

PHX_u32 PHX_Filesystem_FAT_Cluster(PHX_Filesystem_FAT_Version version, PHX_u32 cluster);
PHX_Result PHX_Filesystem_FAT_ReadFAT(PHX_Filesystem_FAT_Data* data, PHX_u32 cluster, PHX_u32* outValue);
PHX_Result PHX_Filesystem_FAT_WriteFAT(PHX_Filesystem_FAT_Data* data, PHX_u32 cluster, PHX_u32 value);

PHX_Result PHX_Filesystem_FAT_ReadRootDirectoryEntries(PHX_Filesystem_FAT_Data* data, PHX_u16 index, PHX_u16 count, PHX_Byte* outEntries);
PHX_Result PHX_Filesystem_FAT_WriteRootDirectoryEntries(PHX_Filesystem_FAT_Data* data, PHX_u16 index, PHX_u16 count, const PHX_Byte* entries);

PHX_Result PHX_Filesystem_FAT_WriteEntries(PHX_Filesystem* fs, PHX_u32 entryCluster, PHX_u32 entryIndex, const void* entries, PHX_u32 totalEntries);

static inline PHX_BlockSize PHX_Filesystem_FAT_GetClusterStart(PHX_Filesystem_FAT_Data* data, PHX_u32 cluster)
{
    return (PHX_u64)(cluster - 2) * (PHX_u64)data->sectorsPerCluster + (PHX_u64)data->dataSector;
}

PHX_Result PHX_Filesystem_FAT_FindFreeClusters(PHX_Filesystem_FAT_Data* data, PHX_u32 count, PHX_u32* outFirstCluster);


void PHX_Filesystem_FAT_LFN_ExtractChars(const PHX_Byte* lfn, PHX_u16 out[13]);
PHX_Byte PHX_Filesystem_FAT_LFN_Checksum(const PHX_Byte shortName[11]);

PHX_Bool PHX_Filesystem_FAT_NameEquals(const char* a, const char* b);
void PHX_Filesystem_FAT_BuildShortName(const PHX_Byte rawName[11], char out[13]);
void PHX_Filesystem_FAT_GetShortNameCharacters(const char* name, const char* nameEnd, PHX_Byte outCount, char* out);

PHX_Result PHX_Filesystem_FAT_GenerateShortName(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char firstChars[8], const char hash[4], const char ext[3], char shortNameOut[11]);

PHX_u32 PHX_Filesystem_FAT_HashName(const char* name);
void PHX_Filesystem_FAT_HashToChars(PHX_u32 hash, char out[4]);

PHX_u32 PHX_Filesystem_FAT_UTF16_To_UTF8(const PHX_u16* units, PHX_u32 count, char* out, PHX_u32 maxOut);
PHX_u32 PHX_Filesystem_FAT_UTF8_To_UTF16(const char* in, PHX_u32 count, PHX_u16* out, PHX_u32 maxOut);


PHX_Result PHX_Filesystem_FAT_ChangeBootsector(PHX_Filesystem* fs, const PHX_Byte* bootsector);
void PHX_Filesystem_FAT_Destroy(PHX_Filesystem* fs);

PHX_Result PHX_Filesystem_FAT_GetNode(PHX_Filesystem* fs, PHX_Filesystem_NodeNumber number, PHX_Filesystem_Node* nodeOut);
PHX_Result PHX_Filesystem_FAT_RemoveNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node);
void PHX_Filesystem_FAT_CleanupNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node);

PHX_Result PHX_Filesystem_FAT_CreateNode(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_Entry_Type type, PHX_Filesystem_Entry_Attribute attributes, const char* name, PHX_Filesystem_Node* nodeOut);

PHX_Result PHX_Filesystem_FAT_LinkEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Node* target);
PHX_Result PHX_Filesystem_FAT_UnlinkEntry(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Size* newReferenceCountOut);

PHX_Result PHX_Filesystem_FAT_CreateOpenNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNodeOut);
PHX_Result PHX_Filesystem_FAT_CloseOpenNode(PHX_Filesystem* fs, PHX_Filesystem_OpenNode* openNode);
PHX_Result PHX_Filesystem_FAT_ResetOpenNode(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNode);


#ifdef __cplusplus
}
#endif

#endif
