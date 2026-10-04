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

void PHX_Filesystem_FAT_LFN_BuildEntry(const PHX_u16* utf16Name, PHX_u32 totalChars, PHX_u32 totalSlots, PHX_u32 slotIndex, PHX_Byte checksum, PHX_Byte* entryOut)
{
    const PHX_u32 order = totalSlots - slotIndex;
    const PHX_Bool isLast = (slotIndex == 0) ? PHX_TRUE : PHX_FALSE;

    write_u8(entryOut + PHX_FILESYSTEM_FAT_LFNENT_ORD, (PHX_Byte)(order | ((isLast == PHX_TRUE) ? 0x40 : 0x00)));
    write_u8(entryOut + PHX_FILESYSTEM_FAT_LFNENT_ATR, PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE);
    write_u8(entryOut + PHX_FILESYSTEM_FAT_LFNENT_RES, 0);
    write_u8(entryOut + PHX_FILESYSTEM_FAT_LFNENT_CHE, checksum);
    write_u16(entryOut + PHX_FILESYSTEM_FAT_LFNENT_RES16, 0);

    PHX_u16 chars[13];
    const PHX_u32 base = (order - 1) * 13;

    for (PHX_u32 i = 0; i < 13; i++)
    {
        const PHX_u32 pos = base + i;
        chars[i] = (pos < totalChars) ? utf16Name[pos] : 0xFFFF;
    }

    memcpy(entryOut + PHX_FILESYSTEM_FAT_LFNENT_NA1, chars, 5 * sizeof(PHX_u16));
    memcpy(entryOut + PHX_FILESYSTEM_FAT_LFNENT_NA2, chars + 5, 6 * sizeof(PHX_u16));
    memcpy(entryOut + PHX_FILESYSTEM_FAT_LFNENT_NA3, chars + 11, 2 * sizeof(PHX_u16));
}
