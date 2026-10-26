#ifndef PHX_DEVICE_FILESYSTEM_H
#define PHX_DEVICE_FILESYSTEM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <phx/types.h>
#include <phx/result.h>
#include <phx/context.h>
#include <phx/device/disk.h>

#define PHX_FILESYSTEM_NO_ID 0xFFFFFFFFFFFFFFFF

typedef PHX_u8 PHX_Filesystem_Node_Type;
#define PHX_FILESYSTEM_NODE_TYPE_FILE      ((PHX_Filesystem_Node_Type)0)
#define PHX_FILESYSTEM_NODE_TYPE_DIRECTORY ((PHX_Filesystem_Node_Type)1)

typedef PHX_u16 PHX_Filesystem_Node_Attribute;
#define PHX_FILESYSTEM_NODE_ATTRIBUTE_READONLY   ((PHX_Filesystem_Node_Attribute)0x01)
#define PHX_FILESYSTEM_NODE_ATTRIBUTE_EXECUTABLE ((PHX_Filesystem_Node_Attribute)0x02)
#define PHX_FILESYSTEM_NODE_ATTRIBUTE_HIDDEN     ((PHX_Filesystem_Node_Attribute)0x04)
#define PHX_FILESYSTEM_NODE_ATTRIBUTE_SYSTEM     ((PHX_Filesystem_Node_Attribute)0x08)

typedef PHX_u64 PHX_Filesystem_Node_Number;
typedef PHX_u64 PHX_Filesystem_Node_Count;
typedef PHX_u64 PHX_Filesystem_Node_Size;

typedef struct PHX_Filesystem_Node
{
    PHX_Filesystem_Node_Number number;
    PHX_Filesystem_Node_Count referenceCount;

    PHX_Filesystem_Node_Size size;

    void* extra;

    PHX_Filesystem_Node_Attribute attributes;
    PHX_Filesystem_Node_Type type;
} PHX_Filesystem_Node;

typedef struct PHX_Filesystem_Entry
{
    PHX_Filesystem_Node_Number node;
    char name[PHX_NAME_LEN + 1];
} PHX_Filesystem_Entry;

typedef struct PHX_Filesystem_OpenNode
{
    PHX_Filesystem_Node_Size pos;
    void* extra;
} PHX_Filesystem_OpenNode;

typedef struct PHX_Filesystem_Device PHX_Filesystem_Device;

typedef struct PHX_Filesystem_Operations
{
    PHX_Filesystem_Node_Number rootNodeNumber;

    // General (TODO: Don't keep two general areas here...)
    PHX_Result (*changeBootsector)(PHX_Filesystem_Device* fs, const PHX_u8* bootsector);
    void (*destroy)(PHX_Filesystem_Device* fs);

    // Node
    PHX_Result (*getNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node_Number number, PHX_Filesystem_Node* nodeOut);
    PHX_Result (*removeNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* node);
    void (*cleanupNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* node);

    // Directories
    PHX_Result (*dir_getEntryCount)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, PHX_u64* entryCountOut);
    PHX_Result (*dir_readEntry)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_OpenNode* openDir, PHX_Filesystem_Entry* entryOut);
    PHX_Result (*dir_lookupEntry)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Entry* entryOut);

    // Files
    PHX_Filesystem_Node_Size (*file_read)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Node_Size size, void* buffer);
    PHX_Filesystem_Node_Size (*file_write)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Node_Size size, const void* buffer);
    PHX_Result (*file_seek)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* file, PHX_Filesystem_OpenNode* openFile, PHX_Filesystem_Node_Size pos);

    // General
    PHX_Result (*createNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, PHX_Filesystem_Node_Type type, PHX_Filesystem_Node_Attribute attributes, const char* name, PHX_Filesystem_Node* nodeOut);

    PHX_Result (*linkEntry)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Node* target);
    PHX_Result (*unlinkEntry)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* dir, const char* name, PHX_Filesystem_Node_Size* newReferenceCountOut);
    PHX_Result (*moveEntry)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* srcDir, const char* srcName, PHX_Filesystem_Node* dstDir, const char* dstName, PHX_Filesystem_Node_Number* newNumberOut);

    PHX_Result (*createOpenNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNodeOut);
    PHX_Result (*closeOpenNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_OpenNode* openNode);
    PHX_Result (*resetOpenNode)(PHX_Filesystem_Device* fs, PHX_Filesystem_Node* node, PHX_Filesystem_OpenNode* openNode);

    // TODO: Info, update node, clear file
} PHX_Filesystem_Operations;

struct PHX_Filesystem_Device
{
    // TODO: name

    PHX_Context* context;
    PHX_Disk_Device* device;

    void* data;
    
    const struct PHX_Filesystem_Operations* ops;

    const char* type;

    PHX_u64 id;

    PHX_Bool readonly;

    PHX_Bool caseSensitive;
};

#ifdef __cplusplus
}
#endif

#endif
