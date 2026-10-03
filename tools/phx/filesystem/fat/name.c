#include "fat.h"
#include "types.h"

void PHX_Filesystem_FAT_BuildShortName(const PHX_Byte rawName[11], char out[13])
{
    PHX_Byte base[8];
    memcpy(base, rawName, 8);
    if (base[0] == PHX_FILESYSTEM_FAT_ENTRY_KANJI_ESCAPE)
        base[0] = 0xE5;

    const PHX_Byte* ext = rawName + 8;

    int len = 0;
    for (int i = 0; i < 8 && base[i] != ' '; i++)
        out[len++] = (char)base[i];

    if (ext[0] != ' ')
    {
        out[len++] = '.';
        for (int i = 0; i < 3 && ext[i] != ' '; i++)
            out[len++] = (char)ext[i];
    }

    out[len] = '\0';
}

PHX_Bool PHX_Filesystem_FAT_NameEquals(const char* a, const char* b)
{
    // TODO: Actually use UTF8

    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';

        if (ca != cb)
            return PHX_FALSE;

        a++;
        b++;
    }

    return (*a == *b) ? PHX_TRUE : PHX_FALSE;
}
