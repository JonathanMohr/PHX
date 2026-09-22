#include "types.h"

PHX_u32 PHX_Context_GetRandomU32(PHX_Context* context)
{
    PHX_u32 x = context->seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    context->seed = x;

    return x;
}
