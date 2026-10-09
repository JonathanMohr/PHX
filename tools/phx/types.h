#ifndef PHX_TYPES_H
#define PHX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PHX_NAME_LEN 512
#define NAME_LEN (PHX_NAME_LEN + 1)

typedef size_t PHX_Size;
typedef uint64_t PHX_BlockSize;
typedef uint64_t PHX_BlockByteSize;
typedef uint64_t PHX_PartitionSize;
typedef uint64_t PHX_AnySize;

#define PHX_BLOCKSIZE_MAX 0xFFFFFFFFFFFFFFFF
#define PHX_PARTITIONSIZE_MAX 0xFFFFFFFFFFFFFFFF


typedef uint8_t PHX_Byte;
typedef uint16_t PHX_u16;
typedef uint32_t PHX_u32;
typedef uint64_t PHX_u64;

typedef int64_t PHX_Time;

typedef bool PHX_Bool;
#define PHX_TRUE true
#define PHX_FALSE false

#define PHX_NULL NULL

#ifdef __cplusplus
}
#endif

#endif
