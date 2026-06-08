#include "gmpak.hpp"
#include <cstring>
#include <cstdlib>

GMPak::GMPak(const char* filename) : entries(nullptr), f(nullptr), entry_count(0), last_index(0) {
    f = fopen(filename, "rb");
    if (!f) return;

    uint8_t header[10];
    if (fread(header, 1, 10, f) != 10) return;
    if (memcmp(header, "GMPK", 4) != 0) return;

    uint32_t toc_off = header[6] | (header[7] << 8) | (header[8] << 16) | (header[9] << 24);
    fseek(f, toc_off, SEEK_SET);

    uint8_t count_buf[4];
    if (fread(count_buf, 1, 4, f) != 4) return;
    entry_count = count_buf[0] | (count_buf[1] << 8) | (count_buf[2] << 16) | (count_buf[3] << 24);

    entries = (GMPakEntry*)malloc(sizeof(GMPakEntry) * entry_count);
    for (uint32_t i = 0; i < entry_count; i++) {
        uint8_t entry_data[25];
        fread(entry_data, 1, 25, f);
        memcpy(entries[i].name, entry_data, 16);
        entries[i].name[16] = '\0';
        entries[i].type = entry_data[16];
        entries[i].offset = entry_data[17] | (entry_data[18] << 8) | (entry_data[19] << 16) | (entry_data[20] << 24);
        entries[i].size = entry_data[21] | (entry_data[22] << 8) | (entry_data[23] << 16) | (entry_data[24] << 24);
        entries[i].cached_data = nullptr;
    }
}

GMPak::~GMPak() {
    if (entries) {
        for (uint32_t i = 0; i < entry_count; i++) {
            if (entries[i].cached_data) free(entries[i].cached_data);
        }
        free(entries);
    }
    if (f) fclose(f);
}

void* GMPak::get_entry(const char* name, uint32_t* size, uint8_t* type) {
    for (uint32_t i = 0; i < entry_count; i++) {
        uint32_t idx = (last_index + i) % entry_count;
        if (strcmp(entries[idx].name, name) == 0) {
            last_index = idx;
            if (size) *size = entries[idx].size;
            if (type) *type = entries[idx].type;
            if (entries[idx].cached_data) return entries[idx].cached_data;

            void* data = malloc(entries[idx].size);
            if (data) {
                fseek(f, entries[idx].offset, SEEK_SET);
                fread(data, 1, entries[idx].size, f);
            }
            return data;
        }
    }
    return nullptr;
}

void GMPak::preload_entries(const char* prefix, int start, int end) {
    size_t prefix_len = strlen(prefix);
    for (uint32_t i = 0; i < entry_count; i++) {
        if (strncmp(entries[i].name, prefix, prefix_len) == 0) {
            int val = atoi(entries[i].name + prefix_len);
            bool in_range = false;
            if (start <= end) in_range = (val >= start && val <= end);
            else in_range = (val >= start || val <= end);

            if (in_range) {
                if (!entries[i].cached_data) {
                    entries[i].cached_data = malloc(entries[i].size);
                    if (entries[i].cached_data) {
                        fseek(f, entries[i].offset, SEEK_SET);
                        fread(entries[i].cached_data, 1, entries[i].size, f);
                    }
                }
            } else {
                if (entries[i].cached_data) {
                    free(entries[i].cached_data);
                    entries[i].cached_data = nullptr;
                }
            }
        }
    }
}
