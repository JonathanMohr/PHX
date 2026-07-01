#include "filesystem.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "../native/directory.h"

char** SeparatePaths(const char* path, uint32_t* out_count)
{
    if (!path) return NULL;

    uint32_t count = 0;
    const char* p = path;
    while (*p) {
        if (*p == '/') count++;
        p++;
    }
    count++; // last

    char** parts = (char**)calloc(count + 1, sizeof(char*));
    if (!parts) return NULL;

    uint32_t i = 0;
    const char* start = path;
    for (p = path; *p; p++) {
        if (*p == '/') {
            uint64_t len = (uint64_t)p - (uint64_t)start;
            if (len > 0) {
                parts[i] = (char*)malloc(len + 1);
                memcpy(parts[i], start, len);
                parts[i][len] = '\0';
                i++;
            }
            start = p + 1;
        }
    }
    if (*start) {
        uint64_t len = strlen(start);
        parts[i] = (char*)malloc(len + 1);
        memcpy(parts[i], start, len + 1);
        i++;
    }

    *out_count = i;
    parts[i] = NULL;
    return parts;
}

Filesystem* Filesystem_CreateFromFAT(FAT_Filesystem* fat_fs, int use_lfn)
{
    Filesystem* fs = (Filesystem*)malloc(sizeof(Filesystem));
    if (!fs) return NULL;
    fs->fat_fs = fat_fs;
    fs->fat_use_lfn = use_lfn;

    fs->static_root.fs = fs;
    fs->static_root.fat_f = fat_fs->root;
    fs->root = &fs->static_root;

    return fs;
}

void Filesystem_Close(Filesystem* fs)
{
    if (!fs) return;
    FAT_CloseFilesystem(fs->fat_fs);
    free(fs);
}

Filesystem_File* Filesystem_CreateEntry(Filesystem_File* parent, const char* name,  int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access)
{
    if (!parent || !name) return NULL;
    Filesystem_File* file = (Filesystem_File*)malloc(sizeof(Filesystem_File));
    if (!file) return NULL;
    file->fs = parent->fs;

    //TODO: dynamically choose if to use lfn
    file->fat_f = FAT_CreateEntry(parent->fat_f, name, is_directory, is_hidden, is_system, creation, last_modification, last_access, parent->fs->fat_use_lfn);
    if (!file->fat_f) {
        free(file);
        return NULL;
    }

    return file;
}

Filesystem_File* Filesystem_FindEntry(Filesystem_File* parent, const char* name)
{
    if (!parent || !name) return NULL;
    Filesystem_File* file = (Filesystem_File*)malloc(sizeof(Filesystem_File));
    if (!file) return NULL;
    file->fs = parent->fs;

    file->fat_f = FAT_FindEntry(parent->fat_f, name);
    if (!file->fat_f) {
        free(file);
        return NULL;
    }

    return file;
}

Filesystem_File* Filesystem_OpenEntry(Filesystem_File* parent, const char* name, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access)
{
    if (!parent || !name) return NULL;
    Filesystem_File* entry = Filesystem_FindEntry(parent, name);
    if (entry) return entry;
    return Filesystem_CreateEntry(parent, name, is_directory, is_hidden, is_system, creation, last_modification, last_access);
}

int Filesystem_DeleteEntry(Filesystem_File* parent, const char* name)
{
    if (!parent || !name) return 1;
    Filesystem_File* entry = Filesystem_FindEntry(parent, name);
    if (!entry || entry->fat_f->is_root_directory || entry->fat_f->is_root_directory_fat32) return 1;
    if (entry->fat_f->is_directory) {
        uint64_t sub_entry_count;
        char** sub_entries = Filesystem_ListDir(entry, &sub_entry_count);
        if (!sub_entries) {
            return 1;
        }
        for (uint64_t i = 0; i < sub_entry_count; i++) {
            free(sub_entries[i]);
        }
        free((void*)sub_entries);

        // FIXME: currently working, but check for . and ..
        if ((sub_entry_count - 2) > 0) return 2;
        return FAT_DeleteEntry(entry->fat_f);
    } else {
        return FAT_DeleteEntry(entry->fat_f);
    }
}

Filesystem_File* Filesystem_OpenPath(Filesystem_File* current_path, const char* path, int create_file, int create_parents, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access)
{
    if (path[0] == '/') {
        current_path = current_path->fs->root;
        path++;
    }

    uint64_t str_len = strlen(path);
    if (str_len == 0) {
        Filesystem_File* f = (Filesystem_File*)malloc(sizeof(Filesystem_File));
        if (!f) return NULL;
        f->fs = current_path->fs;
        FAT_File* fat_f = (FAT_File*)malloc(sizeof(FAT_File));
        if (!fat_f) {
            free(f);
            return NULL;
        }
        memcpy(fat_f, current_path->fat_f, sizeof(FAT_File));
        f->fat_f = fat_f;
        return f;
    }

    uint32_t path_out_count;
    char** entries = SeparatePaths(path, &path_out_count);
    if (!entries) return NULL;

    int skip = 0;
    for (uint32_t i = 0; i < path_out_count; i++) {
        char* name = entries[i];
        if (skip) {
            free(name);
            continue;
        }
        if (!name) {
            if (i > 0) Filesystem_CloseEntry(current_path);
            skip = 1;
            continue;
        }

        Filesystem_File* new_entry;
        if (i+1 < path_out_count) {
            if (create_parents) new_entry = Filesystem_OpenEntry(current_path, name, 1, is_hidden, is_system, creation, last_modification, last_access);
            else                new_entry = Filesystem_FindEntry(current_path, name);
        } else {
            if (create_file) new_entry = Filesystem_OpenEntry(current_path, name, is_directory, is_hidden, is_system, creation, last_modification, last_access);
            else             new_entry = Filesystem_FindEntry(current_path, name);
        }
        if (!new_entry) {
            if (i > 0) Filesystem_CloseEntry(current_path);
            skip = 1;
            continue;
        }
        if (i > 0) Filesystem_CloseEntry(current_path);
        current_path = new_entry;
        free(name);
    }

    free((void*)entries);

    return skip ? NULL : current_path;
}

int Filesystem_DeletePath(Filesystem_File* current_path, const char* path, int save)
{
    if (!current_path || !path) return 1;

    Filesystem_File* entry = Filesystem_OpenPath(current_path, path, 0, 0, 0, 0, 0, 0, 0, 0);

    if (!entry || entry->fat_f->is_root_directory || entry->fat_f->is_root_directory_fat32) return 1;

    if (!entry->fat_f->is_directory) return FAT_DeleteEntry(entry->fat_f);

    uint64_t sub_entry_count = 0;
    char** sub_entries = Filesystem_ListDir(entry, &sub_entry_count);
    if (!sub_entries) return 1;

    if (save)
    {
        //TODO: currently just removing '.' and '..' with that
        int is_empty = (sub_entry_count <= 2);

        for (uint64_t i = 0; i < sub_entry_count; i++)
            free(sub_entries[i]);
        free((void*)sub_entries);

        if (!is_empty) return 2;
        return FAT_DeleteEntry(entry->fat_f);
    }

    for (uint64_t i = 0; i < sub_entry_count; i++) {
        if (strcmp(sub_entries[i], ".") == 0 || strcmp(sub_entries[i], "..") == 0) continue;
        int r = Filesystem_DeletePath(entry, sub_entries[i], save);
        free(sub_entries[i]);

        if (r != 0) {
            // TODO
        }
    }
    free((void*)sub_entries);

    return FAT_DeleteEntry(entry->fat_f);
}

void Filesystem_CloseEntry(Filesystem_File* file)
{
    if (!file) return;
    FAT_CloseEntry(file->fat_f);
    free(file);
}

uint64_t Filesystem_ReadFromFile(Filesystem_File* file, uint64_t offset, uint8_t* buffer, uint64_t size)
{
    if (!file) return 0;
    return FAT_ReadFromFile(file->fat_f, (uint32_t)offset, buffer, (uint32_t)size);
}

uint64_t Filesystem_WriteToFile(Filesystem_File* file, uint64_t offset, uint8_t* buffer, uint64_t size)
{
    if (!file) return 0;
    return FAT_WriteToFile(file->fat_f, (uint32_t)offset, buffer, (uint32_t)size);
}

char** Filesystem_ListDir(Filesystem_File* file, uint64_t* out_count)
{
    if (!file || !out_count || !file->fat_f->is_directory) return NULL;
    return FAT_ListDir(file->fat_f, out_count);
}

int Filesystem_DirectoryReserve(Filesystem_File* dir, char** file_names, uint64_t file_count)
{
    if (!dir || !dir->fat_f->is_directory || !file_names) return 1;

    uint64_t entry_count = 0;
    for (uint64_t i = 0; i < file_count; i++) {
        uint64_t name_len = strlen(file_names[i]);
        uint64_t lfn_count = (name_len + 12) / 13;
        entry_count += lfn_count + 1;
    }

    return FAT_ReserveDirectorySpace(dir->fat_f, (uint32_t)entry_count);
}


// TODO: Don't Override
int Filesystem_SyncPathsToFS(Filesystem_File* dir, const char* path, const char* o_path)
{
    if (!dir || !path || !o_path) return 1;

    PathType type = Path_GetType(o_path);
    int is_hidden = 0; //TODO
    int is_system = 0; //TODO

    int64_t creation = 0;
    int64_t last_modification = 0;
    int64_t last_access = 0;

    if (type == TYPE_FILE) {
        Filesystem_File* fs_f = Filesystem_OpenPath(dir, path, 1, 1, 0, is_hidden, is_system, creation, last_modification, last_access);
        FILE* o_f = fopen(o_path, "rb");

        if (!fs_f || !o_f) {
            if (fs_f) Filesystem_CloseEntry(fs_f);
            if (o_f) fclose(o_f);
            fprintf(stderr, "Couldn't add file '%s' as '%s'\n", o_path, path);
            return 1;
        }

        uint8_t buffer[512];
        uint64_t offset = 0;
        uint64_t read = 1;
        while (read) {
            read = (uint64_t)fread(buffer, 1, 512, o_f);
            if (Filesystem_WriteToFile(fs_f, offset, buffer, read) != read) {
                fprintf(stderr, "Couldn't write %" PRIu64 " bytes to %s\n", read, path);
                fclose(o_f);
                Filesystem_CloseEntry(fs_f);
                return 1;
            }
            offset += read;
        }

        fclose(o_f);
        Filesystem_CloseEntry(fs_f);

    } else if (type == TYPE_DIR) {
        // TODO: better way
        Filesystem_File* tmp_dir = Filesystem_OpenPath(dir, path, 1, 1, 1, is_hidden, is_system, creation, last_modification, last_access);

        uint64_t sub_entry_count;
        char** sub_entries = Path_ListDir(o_path, &sub_entry_count);
        if (!sub_entries) {
            Filesystem_CloseEntry(tmp_dir);
            fprintf(stderr, "Couldn't get entries of '%s'\n", o_path);
            return 1;
        }

        if (Filesystem_DirectoryReserve(tmp_dir, sub_entries, sub_entry_count) != 0) {
            // TODO: Warning?
        }

        if (tmp_dir) Filesystem_CloseEntry(tmp_dir);

        for (uint64_t i = 0; i < sub_entry_count; i++) {
            char* entry = sub_entries[i];

            char* name = strrchr(entry, '/');
            uint64_t path_len = strlen(path);
            uint64_t name_len = strlen(name);
            char* new_path = (char*)malloc(path_len + name_len + 1);
            if (!new_path) {
                printf("Warning: Couldn't allocate memory to sync '%s' to '%s%s'\n", entry, path, name);
                free(entry);
                continue;
            }
            strcpy(new_path, path);
            strcpy(new_path + path_len, name);
            new_path[path_len + name_len] = '\0';

            if (Filesystem_SyncPathsToFS(dir, new_path, entry) != 0) {
                printf("Warning: Couldn't sync '%s' to '%s'\n", entry, new_path);
                free(entry);
                continue;
            }

            free(new_path);
            free(entry);
        }

        free((void*)sub_entries);
    }

    return 0;
}

// TODO: Don't Override
int Filesystem_SyncPathsFromFS(Filesystem_File* dir, const char* path, const char* o_path)
{
    if (!dir || !path || !o_path) return 1;

    Filesystem_File* tmp_f = Filesystem_OpenPath(dir, path, 0, 0, 0, 0, 0, 0, 0, 0);
    if (!tmp_f) return 1;
    PathType type = tmp_f->fat_f->is_directory ? TYPE_DIR : TYPE_FILE;
    Filesystem_CloseEntry(tmp_f);
    int is_hidden = 0; //TODO
    int is_system = 0; //TODO

    int64_t creation = 0;
    int64_t last_modification = 0;
    int64_t last_access = 0;

    if (type == TYPE_FILE) {
        Filesystem_File* fs_f = Filesystem_OpenPath(dir, path, 0, 0, 0, is_hidden, is_system, creation, last_modification, last_access);
        int result = Path_MakeDirsForPath(o_path, 0);
        FILE* o_f = fopen(o_path, "w+b"); //truncate

        if (!fs_f || !o_f || result != 0) {
            if (fs_f) Filesystem_CloseEntry(fs_f);
            if (o_f) fclose(o_f);
            fprintf(stderr, "Couldn't add file '%s' as '%s'\n", o_path, path);
            return 1;
        }

        uint8_t buffer[512];
        uint64_t offset = 0;
        uint64_t read = 1;
        while (read) {
            read = Filesystem_ReadFromFile(fs_f, offset, buffer, 512);
            if (fwrite(buffer, 1, read, o_f) != read) {
                fprintf(stderr, "Couldn't write %" PRIu64 " bytes to %s\n", read, o_path);
                fclose(o_f);
                Filesystem_CloseEntry(fs_f);
                return 1;
            }
            offset += read;
        }

        fclose(o_f);
        Filesystem_CloseEntry(fs_f);

    } else if (type == TYPE_DIR) {
        Filesystem_File* fs_f = Filesystem_OpenPath(dir, path, 0, 0, 0, is_hidden, is_system, creation, last_modification, last_access);
        if (!fs_f) {
            fprintf(stderr, "Couldn't get entries of '%s'\n", o_path);
            return 1;
        }

        int result = Path_MakeDirsForPath(o_path, 1);
        if (result != 0) {
            fprintf(stderr, "Couldn't create dir '%s'\n", o_path);
            return 1;
        }

        uint64_t sub_entry_count;
        char** sub_entries = Filesystem_ListDir(fs_f, &sub_entry_count);
        if (!sub_entries) {
            fprintf(stderr, "Couldn't get entries of '%s'\n", o_path);
            return 1;
        }
        Filesystem_CloseEntry(fs_f);

        for (uint64_t i = 0; i < sub_entry_count; i++) {
            char* entry_name = sub_entries[i];
            if ((entry_name[0] == '.' && entry_name[1] == '\0') || (entry_name[0] == '.' && entry_name[1] == '.' && entry_name[2] == '\0')) {
                free(entry_name);
                continue;
            }

            uint64_t o_path_len = strlen(o_path);
            uint64_t name_len = strlen(entry_name);
            int add_slash = (o_path_len > 0 && o_path[o_path_len - 1] != '/') ? 1 : 0;
            char* full_o_path = (char*)malloc(o_path_len + (uint64_t)add_slash + name_len + 1);
            if (!full_o_path) {
                printf("Warning: Couldn't sync '%s'\n", entry_name);
                free(entry_name);
                continue;
            }
            strcpy(full_o_path, o_path);
            if (add_slash) full_o_path[o_path_len++] = '/';
            strcpy(full_o_path + o_path_len, entry_name);
            full_o_path[o_path_len + name_len] = '\0';

            uint64_t path_len = strlen(path);
            char* new_path = (char*)malloc(path_len + (uint64_t)add_slash + name_len + 1);
            if (!new_path) {
                printf("Warning: Couldn't sync '%s' to FS\n", entry_name);
                free(full_o_path);
                free(entry_name);
                continue;
            }
            strcpy(new_path, path);
            if (add_slash) new_path[path_len++] = '/';
            strcpy(new_path + path_len, entry_name);
            new_path[path_len + name_len] = '\0';

            if (Filesystem_SyncPathsFromFS(dir, new_path, full_o_path) != 0) {
                printf("Warning: Couldn't sync '%s' to '%s'\n", new_path, full_o_path);
            }

            free(new_path);
            free(entry_name);
        }

        free((void*)sub_entries);
    }

    return 0;
}

static int compare_strings(const void* a, const void* b) {
    const char* sa = *(const char**)a;
    const char* sb = *(const char**)b;
    return strcmp(sa, sb);
}

int Filesystem_PrintAll(Filesystem_File* start_dir, const char* name, int indent)
{
    if (!start_dir) return 1;

    PathType type = start_dir->fat_f->is_directory ? TYPE_DIR : TYPE_FILE;

    for (int i = 0; i < indent; i++) fputc(' ', stdout);

    if (type == TYPE_FILE) {
        fputs(name, stdout);
        fputc('\n', stdout);
    } else if (type == TYPE_DIR) {
        uint64_t sub_entry_count;
        char** sub_entries = Filesystem_ListDir(start_dir, &sub_entry_count);
        if (!sub_entries) {
            fprintf(stderr, "Couldn't get entries of '%s'\n", name);
            return 1;
        }

        fputs(name, stdout);
        fputs(":\n", stdout);

        qsort((void*)sub_entries, sub_entry_count, sizeof(char*), compare_strings);

        for (uint64_t i = 0; i < sub_entry_count; i++) {
            char* entry_name = sub_entries[i];
            if ((entry_name[0] == '.' && entry_name[1] == '\0') || (entry_name[0] == '.' && entry_name[1] == '.' && entry_name[2] == '\0')) {
                free(entry_name);
                continue;
            }

            Filesystem_File* entry = Filesystem_OpenPath(start_dir, entry_name, 0, 0, 0, 0, 0, 0, 0, 0);
            if (!entry) {
                fprintf(stderr, "Warning: Couldn't open '%s'\n", name);
            }

            if (Filesystem_PrintAll(entry, entry_name, indent + 2) != 0) {
                fprintf(stderr, "Warning: Couldn't print (entries of) '%s'\n", name);
            }

            Filesystem_CloseEntry(entry);

            free(entry_name);
        }

        free((void*)sub_entries);
    }

    return 0;
}
