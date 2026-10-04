#include "fat.h"

PHX_u32 PHX_Filesystem_FAT_HashName(const char* name)
{
    PHX_u32 hash = 2166136261u;
    while (*name)
    {
        hash ^= (PHX_Byte)*name;
        hash *= 16777619u;
        name++;
    }
    return hash;
}

void PHX_Filesystem_FAT_HashToChars(PHX_u32 hash, char out[4])
{
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_$%'@~!(){}^#&";

    for (int i = 0; i < 4; i++)
    {
        out[i] = alphabet[hash % (sizeof(alphabet) - 1)];
        hash /= (sizeof(alphabet) - 1);
    }
}
