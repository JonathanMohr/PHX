#ifndef PHX_FILESYSTEM_FILESYSTEM_H
#define PHX_FILESYSTEM_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <result.h>

#include <device/device.h>

#define PHX_FILESYSTEM_NO_ID 0xFFFFFFFFFFFFFFFF

typedef PHX_Byte PHX_Filesystem_Entry_Type;
#define PHX_FILESYSTEM_ENTRY_FILE      ((PHX_Filesystem_Entry_Type)0)
#define PHX_FILESYSTEM_ENTRY_DIRECTORY ((PHX_Filesystem_Entry_Type)1)

typedef PHX_u16 PHX_Filesystem_Entry_Attribute;
#define PHX_FILESYSTEM_ATTRIBUTE_READONLY   ((PHX_Filesystem_Entry_Attribute)0x01)
#define PHX_FILESYSTEM_ATTRIBUTE_EXECUTABLE ((PHX_Filesystem_Entry_Attribute)0x02)
#define PHX_FILESYSTEM_ATTRIBUTE_HIDDEN     ((PHX_Filesystem_Entry_Attribute)0x04)
#define PHX_FILESYSTEM_ATTRIBUTE_SYSTEM     ((PHX_Filesystem_Entry_Attribute)0x08)

typedef PHX_u64 PHX_Filesystem_Size;
typedef PHX_u64 PHX_Filesystem_NodeNumber;


typedef struct PHX_Filesystem_Node
{
    PHX_Filesystem_NodeNumber number;
    PHX_Filesystem_Size referenceCount;
    PHX_Filesystem_Size size;

    void* extra;

    PHX_Filesystem_Entry_Attribute attributes;
    PHX_Filesystem_Entry_Type type;
} PHX_Filesystem_Node;

typedef struct PHX_Filesystem_Entry
{
    PHX_Filesystem_NodeNumber node;
    char name[PHX_NAME_LEN + 1];
} PHX_Filesystem_Entry;

typedef struct PHX_Filesystem_OpenNode
{
    PHX_Filesystem_Size pos;
    void* extra;
} PHX_Filesystem_OpenNode;

typedef struct PHX_Filesystem PHX_Filesystem;

struct PHX_Filesystem_Operations
{
    PHX_Result (*changeBootsector)(PHX_Filesystem* fs, const PHX_Byte* bootsector);
    void (*destroy)(PHX_Filesystem* fs);

    // Node
    PHX_Result (*getNode)(PHX_Filesystem* fs, PHX_Filesystem_NodeNumber number, PHX_Filesystem_Node* nodeOut);
    PHX_Result (*removeNode)(PHX_Filesystem* fs, PHX_Filesystem_Node* node);
    void (*cleanupNode)(PHX_Filesystem* fs, PHX_Filesystem_Node* node);

    // Directories
    PHX_Result (*dir_getEntryCount)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_u64* entryCountOut);
    PHX_Result (*dir_readEntry)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_OpenNode* openDir, PHX_Filesystem_Entry* entryOut);
    PHX_Result (*dir_lookupEntry)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Entry* entryOut);

    // Files
    PHX_Filesystem_Size (*file_read)(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size size, void* buffer);
    PHX_Filesystem_Size (*file_write)(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size size, const void* buffer);
    PHX_Result (*file_seek)(PHX_Filesystem* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Size pos);

    // General
    PHX_Result (*createNode)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_Entry_Type type, PHX_Filesystem_Entry_Attribute attributes, const char* name, PHX_Filesystem_Node* nodeOut);

    PHX_Result (*linkEntry)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Node* target);
    PHX_Result (*unlinkEntry)(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Size* newReferenceCountOut);
    PHX_Result (*moveEntry)(PHX_Filesystem* fs, PHX_Filesystem_Node* srcDir, const char* srcName, PHX_Filesystem_Node* dstDir, const char* dstName, PHX_Filesystem_NodeNumber* newNumberOut);

    PHX_Result (*createOpenNode)(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNodeOut);
    PHX_Result (*closeOpenNode)(PHX_Filesystem* fs, PHX_Filesystem_OpenNode* openNode);
    PHX_Result (*resetOpenNode)(PHX_Filesystem* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNode);

    PHX_u64 rootNodeNumber;
};

struct PHX_Filesystem
{
    PHX_Context* context;
    PHX_BlockDevice* device;

    void* data;
    
    const struct PHX_Filesystem_Operations* ops;

    PHX_u64 id;

    PHX_Bool readonly;

    PHX_Bool caseSensitive;
};


typedef struct PHX_Filesystem_Interface
{
    PHX_Result (*openFilesystem)(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs, PHX_Bool readonly);
    PHX_Result (*formatFilesystem)(PHX_Context* context, PHX_BlockDevice* device, PHX_Filesystem* outFs, const PHX_Byte* bootsector);

    const char* name;
    const char* type;
} PHX_Filesystem_Interface;

extern PHX_Filesystem_Interface* PHX_Filesystem_Interfaces[];
extern PHX_Size PHX_Filesystem_InterfaceCount;


#ifdef __cplusplus
}
#endif

#endif
