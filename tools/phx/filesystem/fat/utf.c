#include "fat.h"

PHX_u32 PHX_Filesystem_FAT_UTF16_To_UTF8(const PHX_u16* units, PHX_u32 count, char* out, PHX_u32 maxOut)
{
    PHX_u32 written = 0;
    PHX_u32 i = 0;

    while (i < count)
    {
        if (units[i] == 0)
            break;

        PHX_u32 cp = units[i++];
        PHX_u32 len;

        if (cp >= 0xD800 && cp <= 0xDBFF)
        {
            if (i < count && units[i] >= 0xDC00 && units[i] <= 0xDFFF)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (units[i] - 0xDC00);
                i++;
            }
            else
            {
                cp = 0xFFFD;
            }
        }
        else if (cp >= 0xDC00 && cp <= 0xDFFF)
        {
            cp = 0xFFFD;
        }

        len = (cp < 0x80) ? 1 : ((cp < 0x800) ? 2 : ((cp < 0x10000) ? 3 : 4));

        if (len > maxOut - written)
            break;

        switch (len)
        {
            case 1:
                out[written] = (char)cp;
                break;

            case 2:
                out[written] =     (char)(0xC0 | (cp >> 6));
                out[written + 1] = (char)(0x80 | (cp & 0x3F));
                break;

            case 3:
                out[written] =     (char)(0xE0 | (cp >> 12));
                out[written + 1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[written + 2] = (char)(0x80 | (cp & 0x3F));
                break;

            default:
                out[written] =     (char)(0xF0 | (cp >> 18));
                out[written + 1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                out[written + 2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                out[written + 3] = (char)(0x80 | (cp & 0x3F));
                break;
        }

        written += len;
    }

    return written;
}

PHX_u32 PHX_Filesystem_FAT_UTF8_To_UTF16(const char* in, PHX_u32 count, PHX_u16* out, PHX_u32 maxOut)
{
    PHX_u32 written = 0;
    PHX_u32 i = 0;

    while (i < count)
    {
        PHX_u32 cp;
        PHX_u32 len;
        unsigned char b = (unsigned char)in[i++];

        if (b < 0x80)
        {
            cp = b;
        }
        else
        {
            PHX_u32 need, min, k;
            int invalid = 0;

            if (b >= 0xC2 && b <= 0xDF)      { need = 1; cp = b & 0x1F; min = 0x80;    }
            else if (b >= 0xE0 && b <= 0xEF) { need = 2; cp = b & 0x0F; min = 0x800;   }
            else if (b >= 0xF0 && b <= 0xF4) { need = 3; cp = b & 0x07; min = 0x10000; }
            else                             { need = 0; cp = 0xFFFD;   min = 0;       }

            for (k = 0; k < need; k++)
            {
                if (i < count && (((unsigned char)in[i]) & 0xC0) == 0x80)
                {
                    cp = (cp << 6) | (((unsigned char)in[i]) & 0x3F);
                    i++;
                }
                else
                {
                    invalid = 1;
                    break;
                }
            }

            if (invalid ||
                (need && cp < min) ||
                cp > 0x10FFFF ||
                (cp >= 0xD800 && cp <= 0xDFFF))
            {
                cp = 0xFFFD;
            }
        }

        len = (cp >= 0x10000) ? 2 : 1;

        if (len > maxOut - written)
            break;

        if (out)
        {
            if (len > maxOut - written)
                break;

            if (len == 1)
            {
                out[written] = (PHX_u16)cp;
            }
            else
            {
                cp -= 0x10000;
                out[written]     = (PHX_u16)(0xD800 + (cp >> 10));
                out[written + 1] = (PHX_u16)(0xDC00 + (cp & 0x3FF));
            }
        }

        written += len;
    }

    return written;
}
