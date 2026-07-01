#include "fat.h"

#include <stdlib.h>

// TODO: Check if it still works

int FAT_RemoveDirectoryEntry(FAT_File* f)
{
    if (!f || f->read_only || f->fs->read_only) return 1;

    FAT_DirectoryEntry entry;
    
    uint64_t offset = f->lfn_offset;
    while (1) {
        if (Partition_Read(f->fs->partition, (uint8_t*)&entry, offset, sizeof(FAT_DirectoryEntry)) != sizeof(FAT_DirectoryEntry)) {
            //TODO
        }

        if (entry.attribute != 0x0F) break;
        entry.name[0] = FAT_ENTRY_DELETED;

        if (Partition_Write(f->fs->partition, (uint8_t*)&entry, offset, sizeof(FAT_DirectoryEntry)) != sizeof(FAT_DirectoryEntry)) {
            //TODO
        }

        offset += sizeof(FAT_DirectoryEntry);
    }

    if (FAT_GetDirectoryEntry(f, &entry) != 0) {
        //TODO
    }
    entry.name[0] = FAT_ENTRY_DELETED;
    if (FAT_SetDirectoryEntry(f, &entry) != 0) {
        //TODO
    }

    return 0;
}

uint64_t FAT_AddDirectoryEntry(FAT_File* directory, FAT_DirectoryEntry* entry, FAT_LFNEntry* lfn_entries, uint64_t lfn_count)
{
    if (!directory || directory->fs->read_only || !directory->is_directory || !entry) return 0xFFFFFFFF; // TODO: error

    uint64_t needed = lfn_count + 1;
    uint64_t offset = 0;
    uint64_t run = 0;

    FAT_DirectoryEntry tmp;

    int found = 0;
    while (FAT_ReadFromFileRaw(directory, offset, (uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp)) {
        if ((char)tmp.name[0] == 0x00 || (char)tmp.name[0] == FAT_ENTRY_DELETED) {
            run++;
            if (run == needed) {
                found = 1;
                break;
            }
        } else {
            run = 0;
        }
        offset += sizeof(FAT_DirectoryEntry);
    }

    uint64_t base = offset - ((found ? run-1 : run)*sizeof(FAT_DirectoryEntry));

    for (uint64_t i = 0; i < lfn_count; i++) {
        uint64_t off = base + (i * sizeof(FAT_DirectoryEntry));
        if (FAT_WriteToFileRaw(directory, off, (uint8_t*)&lfn_entries[lfn_count - 1 - i], sizeof(FAT_LFNEntry)) != sizeof(FAT_LFNEntry))
            return 0xFFFFFFFFFFFFFFFF; // critical
    }

    uint64_t short_off = base + (lfn_count*sizeof(FAT_DirectoryEntry));
    if (FAT_WriteToFileRaw(directory, short_off, (uint8_t*)entry, sizeof(FAT_DirectoryEntry)) != sizeof(FAT_DirectoryEntry))
        return 0xFFFFFFFFFFFFFFFF;

    return short_off;
}

int FAT_AddDotsToDirectory(FAT_File* directory, FAT_File* parent)
{
    if (!directory || directory->fs->read_only || !directory->is_directory) return 1;

    FAT_DirectoryEntry dot = {0};
    dot.name[0] = '.';
    dot.name[1] = ' ';
    dot.name[2] = ' ';
    dot.name[3] = ' ';
    dot.name[4] = ' ';
    dot.name[5] = ' ';
    dot.name[6] = ' ';
    dot.name[7] = ' ';
    dot.ext[0] = ' ';
    dot.ext[1] = ' ';
    dot.ext[2] = ' ';
    dot.attribute = FAT_ENTRY_DIRECTORY;
    dot.first_cluster = (uint16_t)directory->first_cluster;
    dot.first_cluster_high = (uint16_t)(directory->first_cluster >> 16);
    uint64_t rel_offset_dot = FAT_AddDirectoryEntry(directory, &dot, NULL, 0);
    (void)rel_offset_dot;

    FAT_DirectoryEntry dotdot = {0};
    dotdot.name[0] = '.';
    dotdot.name[1] = '.';
    dotdot.name[2] = ' ';
    dotdot.name[3] = ' ';
    dotdot.name[4] = ' ';
    dotdot.name[5] = ' ';
    dotdot.name[6] = ' ';
    dotdot.name[7] = ' ';
    dotdot.ext[0] = ' ';
    dotdot.ext[1] = ' ';
    dotdot.ext[2] = ' ';
    dotdot.attribute = FAT_ENTRY_DIRECTORY;
    dotdot.first_cluster = (uint16_t)parent->first_cluster;
    dot.first_cluster_high = (uint16_t)(parent->first_cluster >> 16);
    uint64_t rel_offset_dotdot = FAT_AddDirectoryEntry(directory, &dotdot, NULL, 0);
    (void)rel_offset_dotdot;

    return 0;
}

char** FAT_ListDir(FAT_File* dir, uint64_t* out_count)
{
    if (!dir || !dir->is_directory) return NULL;

    uint64_t offset = 0;
    FAT_DirectoryEntry entry;

    FAT_LFNEntry* lfn_entries = NULL;
    uint32_t lfn_count = 0;

    char** list = NULL;
    uint64_t count = 0;

    while (1) {
        uint64_t r = FAT_ReadFromFileRaw(dir, offset, (uint8_t*)&entry, sizeof(FAT_DirectoryEntry));
        if (r != sizeof(FAT_DirectoryEntry)) break;

        if (entry.name[0] == 0x00) break; // End of directory

        if (entry.name[0] == FAT_ENTRY_DELETED) {
            offset += sizeof(FAT_DirectoryEntry);
            continue;
        }

        if (entry.attribute == 0x0F) {
            FAT_LFNEntry* new_entries = (FAT_LFNEntry*)realloc(lfn_entries, sizeof(FAT_LFNEntry) * (lfn_count + 1));
            if (!new_entries) {
                free(lfn_entries);
                return NULL;
            }
            lfn_entries = new_entries;
            memcpy(&lfn_entries[lfn_count], &entry, sizeof(FAT_LFNEntry));
            lfn_count++;

            offset += sizeof(FAT_DirectoryEntry);
            continue;
        }

        char* name_utf8 = NULL;
        if (lfn_count > 0) {
            uint32_t u16len = 0;
            uint16_t* u16 = FAT_CombineLFN(lfn_entries, lfn_count, &u16len);
            if (u16) {
                uint32_t out_len = utf16_to_utf8(u16, u16len, &name_utf8);
                (void)out_len; // TODO
                free(u16);
            }
            free(lfn_entries);
            lfn_entries = NULL;
            lfn_count = 0;
        }

        if (!name_utf8) {
            int len = 8;
            while (len > 0 && entry.name[len-1] == ' ') len--;
            int ext_len = 3;
            while (ext_len > 0 && entry.ext[ext_len-1] == ' ') ext_len--;

            int total_len = len + (ext_len > 0 ? 1 + ext_len : 0);
            name_utf8 = (char*)malloc((size_t)total_len + 1);
            if (!name_utf8) {
                for (uint64_t i = 0; i < count; i++) free(list[i]);
                free((void*)list);
                return NULL;
            }

            memcpy(name_utf8, entry.name, (size_t)len);
            if (ext_len > 0) {
                name_utf8[len] = '.';
                memcpy(name_utf8 + len + 1, entry.ext, (size_t)ext_len);
            }
            name_utf8[total_len] = '\0';
        }

        char** new_list = (char**)realloc((void*)list, sizeof(char*) * (count + 1));
        if (!new_list) {
            free(name_utf8);
            for (uint64_t i = 0; i < count; i++) free(list[i]);
            free((void*)list);
            return NULL;
        }
        list = new_list;
        list[count++] = name_utf8;

        offset += sizeof(FAT_DirectoryEntry);
    }

    if (!list) list = (char**)malloc(8);

    *out_count = count;
    return list;
}
