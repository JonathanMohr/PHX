#include <phx/native/memory.h>

#include <stdlib.h>

static void* PHX_Allocate(struct PHX_Allocator* allocator, PHX_Size size)
{
    (void)allocator;
    void* r = malloc(size);
    return r;
}

static void* PHX_Reallocate(struct PHX_Allocator* allocator, void* oldPtr, PHX_Size newSize)
{
    (void)allocator;
    return realloc(oldPtr, newSize);
}

static void PHX_Free(struct PHX_Allocator* allocator, void* ptr)
{
    (void)allocator;
    free(ptr);
}

struct PHX_Allocator PHX_Native_Allocator = {
    PHX_Allocate,
    PHX_Reallocate,
    PHX_Free,
    NULL
};
