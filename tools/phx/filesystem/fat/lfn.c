#include "fat.h"

void PHX_Filesystem_FAT_LFN_ExtractChars(const PHX_Byte* lfn, PHX_u16 out[13])
{
    memcpy(out, lfn + PHX_FILESYSTEM_FAT_LFNENT_NA1, 5 * sizeof(PHX_u16));
    memcpy(out + 5, lfn + PHX_FILESYSTEM_FAT_LFNENT_NA2, 6 * sizeof(PHX_u16));
    memcpy(out + 11, lfn + PHX_FILESYSTEM_FAT_LFNENT_NA3, 2 * sizeof(PHX_u16));
}

PHX_Byte PHX_Filesystem_FAT_LFN_Checksum(const PHX_Byte shortName[11])
{
    PHX_Byte sum = 0;
    for (int i = 0; i < 11; i++)
        sum = (PHX_Byte)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + shortName[i]);
    return sum;
}
