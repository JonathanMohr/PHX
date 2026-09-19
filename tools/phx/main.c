#include <stdio.h>
#include <stdlib.h>

#include "device/device.h"
#include "file.h"
#include "types.h"

static void* PHX_Allocate(struct PHX_Allocator* allocator, PHX_Size size)
{
    (void)allocator;
    return malloc(size);
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

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    struct PHX_Allocator allocator = {
        PHX_Allocate,
        PHX_Reallocate,
        PHX_Free,
        NULL
    };

    PHX_Context context = {
        PHX_TRUE,
        allocator
    };

    const char* file = argv[1];
    PHX_BlockDevice fileDevice;
    if (PHX_File_Open(file, PHX_FALSE, &fileDevice, 1024 * 1024 * 1024) != PHX_TRUE)
    {
        fprintf(stderr, "Could not open file %s\n", file);
        return 1;
    }

    PHX_BlockDevice diskDevice;
    for (PHX_Size i = 0; i < PHX_Disk_InterfaceCount; i++)
    {
        PHX_Disk_Interface* interface = PHX_Disk_Interfaces[i];
        printf("Formatting with %s...\n", interface->name);

        if (interface->formatDevice(&context, &fileDevice, PHX_FALSE, &diskDevice) != PHX_TRUE)
        {
            printf("Image could not be formatted for %s\n", interface->type);
        }
        else
            break;
    }

    return 0;
}
