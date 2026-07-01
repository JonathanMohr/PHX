#include "fat.h"

#include <string.h>
#include <stdlib.h>

uint64_t FAT_ReadFromFileRaw(FAT_File* f, uint64_t offset, void* buffer, uint64_t size)
{
    if (!f || !buffer) return 0;
    if (offset >= f->size) return 0;

    uint64_t remaining = size;
    uint64_t file_pos = offset;
    void* out = buffer;

    if (offset + size > f->size) remaining = f->size - offset;

    if (f->is_root_directory) {
        uint64_t abs = f->fs->root_offset + offset;
        if (Partition_Read(f->fs->partition, out, abs, remaining) != remaining) return 0;
        return (uint32_t)remaining;
    }

    uint32_t cluster = f->first_cluster;
    if (cluster < 2) return 0;

    uint64_t skip = file_pos / f->fs->cluster_size;
    uint64_t off = file_pos % f->fs->cluster_size;

    while (skip--) {
        cluster = FAT_ReadFATEntry(f->fs, cluster);
        if (FAT_ClusterType(f->fs, cluster) == FAT_CLUSTER_EOC) return 0;
    }

    uint64_t total_read = 0;
    while (remaining > 0 && FAT_ClusterType(f->fs, cluster) != FAT_CLUSTER_EOC) {
        uint64_t abs = f->fs->data_offset + (cluster - 2) * f->fs->cluster_size + off;
        uint64_t chunk = f->fs->cluster_size - off;
        if (chunk > remaining) chunk = remaining;

        if (Partition_Read(f->fs->partition, out, abs, chunk) != chunk) break;

        out = (char*)out + chunk;
        remaining -= chunk;
        total_read += chunk;
        off = 0;

        if (remaining > 0) cluster = FAT_ReadFATEntry(f->fs, cluster);
    }

    return total_read;
}

uint64_t FAT_WriteToFileRaw(FAT_File* f, uint64_t offset, void* buffer, uint64_t size)
{
    if (!f || !buffer || f->read_only || f->fs->read_only) return 0;

    if (f->size > 0) f->changed = 1;

    if (f->is_root_directory) {
        uint64_t abs = f->fs->root_offset + offset;
        if (offset > f->size) return 0;
        uint64_t remaining = size;
        if (offset + remaining > f->size) remaining = f->size - offset;
        if (Partition_Write(f->fs->partition, (void*)buffer, abs, remaining) != remaining) return 0;
        return remaining;
    }

    void* out = buffer;

    uint64_t remaining = size;
    if (offset + remaining > f->size) {
        uint64_t old_end = f->size;
        uint64_t reserve = offset + remaining - f->size;
        // TODO
        int r = FAT_ReserveSpace(f, reserve, !f->is_directory);
        if (r != 0) return 0;
        if (offset > old_end) {
            // fill in the rest
            uint64_t uninitialized = offset - old_end;
            uint8_t zero_buffer[CHUNK_SIZE] = {0};
            
            uint64_t off = old_end;
            while (uninitialized > 0) {
                uint64_t chunk = (uninitialized > CHUNK_SIZE) ? CHUNK_SIZE : uninitialized;
                // TODO: set to zero with a better way
                if (FAT_WriteToFileRaw(f, off, zero_buffer, chunk) != chunk) {
                    // warning
                }
                off += chunk;
                uninitialized -= chunk;
            }
        }
    }

    uint32_t cluster = f->first_cluster;
    if (cluster < 2) return 0;

    uint64_t skip = offset / f->fs->cluster_size;
    uint64_t off = offset % f->fs->cluster_size;

    while (skip--) {
        cluster = FAT_ReadFATEntry(f->fs, cluster);
        if (FAT_ClusterType(f->fs, cluster) == FAT_CLUSTER_EOC) return 0;
    }

    uint64_t written = 0;

    while (remaining > 0 && FAT_ClusterType(f->fs, cluster) != FAT_CLUSTER_EOC) {
        uint64_t abs = f->fs->data_offset + (cluster - 2) * f->fs->cluster_size + off;
        uint64_t chunk = f->fs->cluster_size - off;
        if (chunk > remaining) chunk = remaining;

        if (Partition_Write(f->fs->partition, (void*)out, abs, chunk) != chunk) break;

        out = (char*)out + chunk;
        written += chunk;
        remaining -= chunk;
        off = 0;

        if (remaining > 0) cluster = FAT_ReadFATEntry(f->fs, cluster);
    }

    return written;
}

int FAT_ReserveSpace(FAT_File* f, uint64_t extra, int update_entry_size)
{
    if (!f || f->is_root_directory || f->read_only || f->fs->read_only) return 1;

    uint64_t total_size = f->size + extra;
    uint32_t needed_clusters = (uint32_t)((total_size + f->fs->cluster_size - 1) / f->fs->cluster_size);
    uint32_t current_clusters = (uint32_t)((f->size + f->fs->cluster_size - 1) / f->fs->cluster_size);

    FAT_DirectoryEntry entry;
    if (!f->is_root_directory_fat32) {
        if (FAT_GetDirectoryEntry(f, &entry) != 0) return 1;
    }

    if (update_entry_size) entry.file_size = (uint32_t)total_size;

    if (needed_clusters > current_clusters) {
        uint32_t new_clusters = needed_clusters - current_clusters;
        f->changed = 1;

        uint32_t* clusters = (uint32_t*)malloc(new_clusters * sizeof(uint32_t));
        if (!clusters) return 1;
        if (FAT_FindFreeClusters(f->fs, clusters, new_clusters) != 0) {
            free(clusters);
            return 1;
        }

        if (f->fs->version == FAT_VERSION_32) {
            f->fs->fs_info.free_cluster_count -= new_clusters;
            f->fs->fs_info.next_free_cluster = clusters[new_clusters-1]; //FIXME: somehow working, I don't know why
        }

        uint32_t cluster = f->first_cluster;
        if (cluster == 0) {
            f->first_cluster = clusters[0];

            if (f->is_root_directory_fat32) {
                f->fs->bootsector.fat32.header.root_cluster = clusters[0];
            } else {
                if (f->fs->version == FAT_VERSION_32) {
                    entry.first_cluster = (uint16_t)clusters[0];
                    entry.first_cluster_high = (uint16_t)(clusters[0] >> 16);
                } else {
                    entry.first_cluster = (uint16_t)clusters[0];
                }
            }
        } else {
            uint32_t last = f->first_cluster;
            while (1) {
                uint32_t next = FAT_ReadFATEntry(f->fs, last);
                if (FAT_ClusterType(f->fs, next) == FAT_CLUSTER_EOC) break;
                last = next;
            }
            FAT_WriteFATEntry(f->fs, last, clusters[0]);
        }

        for (uint32_t i = 0; i < new_clusters; i++) {
            uint32_t next = (i + 1 < new_clusters) ? clusters[i + 1] : FAT_GetEOF(f->fs);
            FAT_WriteFATEntry(f->fs, clusters[i], next);

            if (!f->is_directory) continue;

            uint8_t* zero_buf = (uint8_t*)calloc(1, f->fs->cluster_size);
            if (!zero_buf) {
                // TODO
            }

            uint64_t cluster_offset = f->fs->data_offset + ((uint64_t)(clusters[i] - 2) * f->fs->cluster_size);
            if (Partition_Write(f->fs->partition, zero_buf, cluster_offset, f->fs->cluster_size) != 0) {
                // TODO
            }

            free(zero_buf);
        }

        free(clusters);
    }

    if (!f->is_root_directory_fat32) {
        if (FAT_SetDirectoryEntry(f, &entry) != 0) return 1;
    }
    f->size = total_size;

    return 0;
}

int FAT_ReserveDirectorySpace(FAT_File* dir, uint64_t entry_count)
{
    if (!dir) return 1;

    uint64_t entry_size = entry_count * sizeof(FAT_DirectoryEntry);
    if (dir->size > entry_size) return 0;

    uint64_t extra_size = entry_size - dir->size;

    return FAT_ReserveSpace(dir, extra_size, !dir->is_directory);
}

uint64_t FAT_GetAbsoluteOffset(FAT_File* f, uint64_t relative_offset)
{
    if (!f) return 0;

    if (relative_offset > f->size) return 0;

    if (f->is_root_directory) {
        return f->fs->root_offset + relative_offset;
    }

    uint32_t cluster = f->first_cluster;
    if (cluster < 2) return 0;

    uint64_t skip = relative_offset / f->fs->cluster_size;
    uint64_t off = relative_offset % f->fs->cluster_size;

    while (skip--) {
        cluster = FAT_ReadFATEntry(f->fs, cluster);
        if (FAT_ClusterType(f->fs, cluster) == FAT_CLUSTER_EOC) return 0;
    }

    return f->fs->data_offset + (cluster - 2) * f->fs->cluster_size + off;
}

int FAT_GetDirectoryEntry(FAT_File* f, FAT_DirectoryEntry* entry)
{
    if (!f || !entry) return 1;
    if (Partition_Read(f->fs->partition, (void*)entry, f->directory_entry_offset, sizeof(FAT_DirectoryEntry)) != sizeof(FAT_DirectoryEntry)) return 1;
    return 0;
}

int FAT_SetDirectoryEntry(FAT_File* f, FAT_DirectoryEntry* entry)
{
    if (!f || f->read_only || f->fs->read_only || !entry) return 1;
    if (Partition_Write(f->fs->partition, (void*)entry, f->directory_entry_offset, sizeof(FAT_DirectoryEntry)) != sizeof(FAT_DirectoryEntry)) return 1;
    return 0;
}

FAT_File* FAT_CreateEntryRaw(FAT_File* dir, FAT_DirectoryEntry* entry, int is_directory, FAT_LFNEntry* lfn_entries, uint32_t lfn_count)
{
    if (!dir || dir->fs->read_only || !entry) return NULL;

    FAT_File* f = (FAT_File*)malloc(sizeof(FAT_File));
    if (!f) return NULL;
    
    uint64_t rel_offset = FAT_AddDirectoryEntry(dir, entry, lfn_entries, lfn_count);
    if (rel_offset == 0xFFFFFFFFFFFFFFFF)
    {
        free(f);
        return NULL;
    }

    f->fs = dir->fs;
    f->size = 0;
    f->first_cluster = 0;
    f->directory_entry_offset = FAT_GetAbsoluteOffset(dir, rel_offset);
    f->lfn_offset = f->directory_entry_offset - (lfn_count * sizeof(FAT_LFNEntry));
    f->is_root_directory = 0;
    f->is_root_directory_fat32 = 0;
    f->is_directory = is_directory;
    f->changed = 1;

    return f;
}

void FAT_CloseEntry(FAT_File* entry)
{
    if (!entry || entry->fs->read_only) return;

    if (entry->changed && !entry->is_root_directory && !entry->is_root_directory_fat32) {
        FAT_DirectoryEntry dir_entry;
        if (FAT_GetDirectoryEntry(entry, &dir_entry) != 0) {
            //TODO: Error
        }
        if (!entry->is_directory) dir_entry.attribute |= FAT_ENTRY_ARCHIVE;

        if (entry->read_only) dir_entry.attribute |= FAT_ENTRY_READ_ONLY;
        else dir_entry.attribute &= (uint8_t)~(int8_t)FAT_ENTRY_READ_ONLY;

        if (entry->is_hidden) dir_entry.attribute |= FAT_ENTRY_HIDDEN;
        else dir_entry.attribute &= (uint8_t)~(int8_t)FAT_ENTRY_HIDDEN;

        if (entry->is_system) dir_entry.attribute |= FAT_ENTRY_SYSTEM;
        else dir_entry.attribute &= (uint8_t)~(int8_t)FAT_ENTRY_SYSTEM;

        if (FAT_SetDirectoryEntry(entry, &dir_entry) != 0) {
            //TODO: Error
        }
    }

    free(entry);
}

FAT_File* FAT_CreateEntry(FAT_File* parent, const char* name, int is_directory, int is_hidden, int is_system, int64_t creation, int64_t last_modification, int64_t last_access, int use_lfn)
{
    if (!parent || parent->fs->read_only || !parent->is_directory || !name) return NULL;

    FAT_DirectoryEntry entry;
    if (FAT_ParseName(name, entry.name, entry.ext) != 0) return NULL;

    entry.attribute = is_directory ? FAT_ENTRY_DIRECTORY : FAT_ENTRY_ARCHIVE;
    if (is_hidden) entry.attribute |= FAT_ENTRY_HIDDEN;
    if (is_system) entry.attribute |= FAT_ENTRY_SYSTEM;
    entry.reserved = 0;
    entry.first_cluster_high = 0;
    entry.first_cluster = 0;
    entry.file_size = 0;

    uint16_t creation_date = entry.creation_date;
    uint16_t creation_time = entry.creation_time;
    uint8_t  creation_time_tenths = entry.creation_time_tenths;

    FAT_EncodeTime(creation, &creation_date, &creation_time, &creation_time_tenths);

    entry.creation_date = creation_date;
    entry.creation_time = creation_time;
    entry.creation_time_tenths = creation_time_tenths;

    uint16_t last_mod_date = entry.last_modification_date;
    uint16_t last_mod_time = entry.last_modification_time;
    uint8_t  last_mod_tenths = 0;

    FAT_EncodeTime(last_modification, &last_mod_date, &last_mod_time, &last_mod_tenths);

    entry.last_modification_date = last_mod_date;
    entry.last_modification_time = last_mod_time;

    uint16_t last_access_time = 0;
    uint16_t last_access_date = entry.last_access_date;
    uint8_t last_access_tenths = 0;

    FAT_EncodeTime(last_access, &last_access_date, &last_access_time, &last_access_tenths);

    entry.last_access_date = last_access_date;

    // TODO: Delete LFN Entries

    uint8_t checksum = FAT_CreateChecksum(&entry);
    uint32_t lfn_count = 0;
    FAT_LFNEntry* lfn_entries = NULL;
    if (use_lfn) {
        lfn_entries = FAT_CreateLFNEntries(name, &lfn_count, checksum);
        if (!lfn_entries) return NULL;
    }

    FAT_File* file = FAT_CreateEntryRaw(parent, &entry, is_directory, lfn_entries, lfn_count);
    if (!file) return NULL;

    file->read_only = 0;
    file->is_hidden = is_hidden;
    file->is_system = is_system;

    if (lfn_entries) free(lfn_entries);

    if (is_directory) FAT_AddDotsToDirectory(file, parent);

    return file;
}

FAT_File* FAT_FindEntry(FAT_File* parent, const char* name)
{
    if (!parent || !parent->is_directory || !name) return NULL;

    uint16_t* name16 = NULL;
    uint32_t nameLen = utf8_to_utf16(name, &name16);

    char short_name[8];
    char short_ext[3];
    if (FAT_ParseName(name, short_name, short_ext) != 0) return NULL;

    uint32_t offset = 0;
    FAT_DirectoryEntry entry;

    FAT_LFNEntry* lfn_entries = NULL;
    uint32_t lfn_count = 0;

    while(1) {
        uint64_t r = FAT_ReadFromFileRaw(parent, offset, (void*)&entry, sizeof(FAT_DirectoryEntry));
        if (r != sizeof(FAT_DirectoryEntry)) break;

        if ((char)entry.name[0] == 0x00) break; // End of directory

        if ((char)entry.name[0] == FAT_ENTRY_DELETED) {
            offset += sizeof(FAT_DirectoryEntry);
            continue;
        }

        if (entry.attribute == 0x0F) {
            
            FAT_LFNEntry* new_entries = (FAT_LFNEntry*)realloc(lfn_entries, sizeof(FAT_LFNEntry) * (lfn_count + 1));
            if (!new_entries) {
                free(lfn_entries);
                free(name16);
                return NULL;
            }
            lfn_entries = new_entries;
            memcpy(&lfn_entries[lfn_count], &entry, sizeof(FAT_LFNEntry));
            lfn_count++;

            offset += sizeof(FAT_DirectoryEntry);
            continue;
        }

        int usedLFN = lfn_count > 0;

        if (lfn_count > 0) {
            uint32_t utf16_len = 0;
            uint16_t* lfn_utf16 = FAT_CombineLFN(lfn_entries, lfn_count, &utf16_len);

            if (lfn_utf16) {
                if (utf16_len == nameLen && utf16_case_insensitive_equal(name16, lfn_utf16, utf16_len)) {
                    free(name16);
                    free(lfn_utf16);
                    free(lfn_entries);

                    FAT_File* file = (FAT_File*)malloc(sizeof(FAT_File));
                    if (!file) return NULL;

                    file->fs = parent->fs;
                    file->first_cluster = ((parent->fs->version == FAT_VERSION_32) ? ((uint32_t)entry.first_cluster_high << 16) : 0) | entry.first_cluster;
                    file->directory_entry_offset = FAT_GetAbsoluteOffset(parent, offset);
                    file->lfn_offset = file->directory_entry_offset - (lfn_count * sizeof(FAT_LFNEntry));
                    file->is_root_directory = 0;
                    file->is_root_directory_fat32 = 0;
                    file->read_only = (entry.attribute & FAT_ENTRY_READ_ONLY) != 0;
                    file->is_hidden = (entry.attribute & FAT_ENTRY_HIDDEN) != 0;
                    file->is_system = (entry.attribute & FAT_ENTRY_SYSTEM) != 0;
                    file->changed = 0;

                    if (entry.attribute & FAT_ENTRY_DIRECTORY) {
                        uint32_t cluster_count = 0;
                        uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster;
                        while (FAT_ClusterType(parent->fs, cluster) != FAT_CLUSTER_EOC) {
                            if (cluster == 0) break;
                            cluster_count++;
                            cluster = FAT_ReadFATEntry(file->fs, cluster);
                        }
                        file->size = (uint32_t)(file->fs->cluster_size * cluster_count);
                        file->is_directory = 1;
                    } else {
                        file->size = entry.file_size;
                        file->is_directory = 0;
                    }

                    return file;
                }
            } else {
                // TODO: warning
            }

            free(lfn_entries);
            lfn_entries = NULL;
            lfn_count = 0;
        }

        if (!usedLFN && memcmp(entry.name, short_name, 8) == 0 && memcmp(entry.ext, short_ext, 3) == 0) {
            free(name16);
            free(lfn_entries);

            FAT_File* file = (FAT_File*)malloc(sizeof(FAT_File));
            if (!file) return NULL;

            file->fs = parent->fs;
            file->first_cluster = ((parent->fs->version == FAT_VERSION_32) ? ((uint32_t)entry.first_cluster_high << 16) : 0) | entry.first_cluster;
            file->directory_entry_offset = FAT_GetAbsoluteOffset(parent, offset);
            file->lfn_offset = file->directory_entry_offset - (lfn_count * sizeof(FAT_LFNEntry));
            file->is_root_directory = 0;
            file->is_root_directory_fat32 = 0;
            file->read_only = (entry.attribute & FAT_ENTRY_READ_ONLY) != 0;
            file->is_hidden = (entry.attribute & FAT_ENTRY_HIDDEN) != 0;
            file->is_system = (entry.attribute & FAT_ENTRY_SYSTEM) != 0;
            file->changed = 0;

            if (entry.attribute & FAT_ENTRY_DIRECTORY) {
                uint32_t cluster_count = 0;
                uint32_t cluster = ((uint32_t)entry.first_cluster_high << 16) | entry.first_cluster;
                while (FAT_ClusterType(parent->fs, cluster) != FAT_CLUSTER_EOC) {
                    cluster_count++;
                    cluster = FAT_ReadFATEntry(file->fs, cluster);
                }
                file->size = (uint32_t)(file->fs->cluster_size * cluster_count);
                file->is_directory = 1;
            } else {
                file->size = entry.file_size;
                file->is_directory = 0;
            }

            return file;
        }

        offset += sizeof(entry);
    }

    free(name16);
    free(lfn_entries);

    return NULL;
}

int FAT_DeleteEntry(FAT_File* f)
{
    if (!f || f->read_only || f->fs->read_only) return 1;

    if (FAT_RemoveFATEntries(f) != 0) {
        return 1;
    }

    if (FAT_RemoveDirectoryEntry(f) != 0) {
        // TODO
    }

    FAT_CloseEntry(f);
    return 0;
}
