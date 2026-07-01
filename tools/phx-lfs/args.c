#include "args.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

static uint64_t parse_size(const char* size_str)
{
    if (!size_str) return 0;

    uint64_t len = strlen(size_str);
    if (len == 0) return 0;

    uint64_t multiplier = 1;
    char last = size_str[len - 1];
    switch (toupper(last)) {
        case 'T': multiplier = 1024ULL*1024*1024*1024; len--; break;
        case 'G': multiplier = 1024ULL*1024*1024; len--; break;
        case 'M': multiplier = 1024ULL*1024; len--; break;
        case 'K': multiplier = 1024ULL; len--; break;
        case 'B': default: multiplier = 1; break;
    }

    uint64_t whole = 0;
    uint64_t fraction = 0;
    uint64_t frac_divisor = 1;
    int after_dot = 0;

    for (size_t i = 0; i < len; i++) {
        char c = size_str[i];
        if (c == '.') {
            after_dot = 1;
            continue;
        }
        if (!isdigit(c)) break;

        if (!after_dot) {
            whole = whole * 10 + (uint64_t)(unsigned char)(c - '0');
        } else {
            if (frac_divisor < 1000000000000000000ULL) {
                fraction = fraction * 10 + (uint64_t)(unsigned char)(c - '0');
                frac_divisor *= 10;
            }
        }
    }

    uint64_t result = whole * multiplier;
    if (fraction > 0) {
        result += (fraction * multiplier) / frac_divisor;
    }

    return result;
}

int ParseArguments(int argc, const char* argv[], int start, Arguments* args)
{
    for (int i = start; i < argc; i++) {
        if (ARG_CMP(i, "--path")) {
            i++;
            if (argc <= i) {
                fputs("No path after '--path'\n", stderr);
                return 1;
            }
            args->path = argv[i];
        } else if (ARG_CMP(i, "--root")) {
            i++;
            if (argc <= i) {
                fputs("No path after '--root'\n", stderr);
                return 1;
            }
            args->root_path = argv[i];
        } else if (ARG_CMP(i, "--boot")) {
            i++;
            if (argc <= i) {
                fputs("No file after '--boot'\n", stderr);
                return 1;
            }
            args->boot_file = argv[i];
        } else if (ARG_CMP(i, "--size")) {
            i++;
            if (argc <= i) {
                fputs("No size after '--size'\n", stderr);
                return 1;
            }
            args->size = parse_size(argv[i]);
        } else if (ARG_CMP(i, "--start")) {
            i++;
            if (argc <= i) {
                fputs("No size after '--start'\n", stderr);
                return 1;
            }
            args->start = parse_size(argv[i]);
        }

        else if (ARG_CMP(i, "--no-lfn")) args->flag_no_lfn = 1;
        else if (ARG_CMP(i, "--read-only")) args->flag_read_only = 1;
        else if (ARG_CMP(i, "--force-bootsector")) args->flag_force_bootsector = 1;
        else if (ARG_CMP(i, "--fast")) args->flag_fast = 1;
        else if (ARG_CMP(i, "--safe")) args->flag_safe = 1;
        else if (ARG_CMP(i, "--count-clusters")) args->flag_count_clusters = 1;
        else if (ARG_CMP(i, "--dont-update-part-entry")) args->flag_dont_update_partition_entry = 1;

        else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
        }
    }
    return 0;
}
