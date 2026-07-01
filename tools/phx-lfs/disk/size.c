#include "disk.h"

uint64_t Path_GetSize(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    if (FileSeek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }

    file_offset_t size = FileTell(f);
    if (FileSeek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return 0;
    }

    fclose(f);

    if (size < 0) return 0;
    return (uint64_t)size;
}
