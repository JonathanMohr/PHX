#ifndef PHX_TYPES_H
#define PHX_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef size_t PHX_Size;
typedef uint64_t PHX_BlockSize;

typedef bool PHX_Bool;
#define PHX_TRUE true
#define PHX_FALSE false

#define PHX_NULL NULL

typedef struct
{
    PHX_Bool fast;
} PHX_Context;

#ifdef __cplusplus
}
#endif

#endif
