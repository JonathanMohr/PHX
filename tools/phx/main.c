#include <stdio.h>

#include "disk/vdi.h"

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <vdi-file>\n", argv[0]);
        return 1;
    }

    const char* file = argv[1];

    Disk disk;
    if (!VDI_ReadDisk(&disk, file, false))
    {
        fputs("Error: Could not read VDI disk\n", stderr);
        return 1;
    }

    disk.close(&disk);

    return 0;
}
