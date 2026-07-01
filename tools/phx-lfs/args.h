#pragma once

#define ARG_CMP(n, str) (strcmp(argv[n], str) == 0)
#define ARG_IS_HELP(n) (ARG_CMP(n, "help") || ARG_CMP(n, "-h") || ARG_CMP(n, "--help"))
#define ARG_IS_VERSION(n) (ARG_CMP(n, "version") || ARG_CMP(n, "-v") || ARG_CMP(n, "--version"))

#include <stdint.h>

typedef struct Arguments
{
    const char* boot_file;
    const char* root_path;
    const char* path;
    uint64_t size;
    uint64_t start;
    int flag_no_lfn;
    int flag_read_only;
    int flag_force_bootsector;
    int flag_fast;
    int flag_safe;
    int flag_count_clusters;
    int flag_dont_update_partition_entry;
} Arguments;

int ParseArguments(int argc, const char* argv[], int start, Arguments* args);
