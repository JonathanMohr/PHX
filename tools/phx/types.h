#ifndef PHX_TYPES_H
#define PHX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define NAME_LEN 128

typedef size_t PHX_Size;
typedef uint64_t PHX_BlockSize;
typedef uint64_t PHX_BlockByteSize;
typedef uint64_t PHX_PartitionSize;

typedef uint8_t PHX_Byte;
typedef uint16_t PHX_u16;
typedef uint32_t PHX_u32;
typedef uint64_t PHX_u64;

typedef bool PHX_Bool;
#define PHX_TRUE true
#define PHX_FALSE false

#define PHX_NULL NULL

struct PHX_Allocator
{
    void* (*allocate)(struct PHX_Allocator* allocator, PHX_Size size);
    void* (*reallocate)(struct PHX_Allocator* allocator, void* oldPtr, PHX_Size newSize);
    void (*free)(struct PHX_Allocator* allocator, void* ptr);
    void* data;
};

typedef struct PHX_Context
{
    PHX_Bool fast;
    struct PHX_Allocator allocator;
} PHX_Context;

#ifdef __cplusplus
}
#endif

#endif
