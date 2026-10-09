#ifndef PHX_CONTEXT_H
#define PHX_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <types.h>
#include <result.h>


struct PHX_Allocator
{
    void* (*allocate)(struct PHX_Allocator* allocator, PHX_Size size);
    void* (*reallocate)(struct PHX_Allocator* allocator, void* oldPtr, PHX_Size newSize);
    void (*free)(struct PHX_Allocator* allocator, void* ptr);
    void* data;
};

typedef struct PHX_Context
{
    void (*printWarning)(PHX_Warning warning);

    PHX_Time currentTime;

    PHX_u32 seed;

    PHX_Bool fast;

    struct PHX_Allocator allocator;
} PHX_Context;

PHX_u32 PHX_Context_GetRandomU32(PHX_Context* context);


#ifdef __cplusplus
}
#endif

#endif
