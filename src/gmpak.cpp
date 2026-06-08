#include "gmpak.hpp"
#include <cstring>
#include <cstdlib>

GMPak::GMPak(const char* filename) : entries(nullptr), entry_count(0), f(nullptr) {
    f = fopen(filename, "rb");
    if (!f) return;

    uint8_t header[10];
    uint32_t toc_off;

    if (fread(header, 1, 10, f) != 10) goto error;
    if (memcmp(header, "GMPK", 4) != 0) goto error;

    toc_off = header[6] | (header[7] << 8) | (header[8] << 16) | (header[9] << 24);

    if (fseek(f, toc_off, SEEK_SET) != 0) goto error;

    uint8_t count_bytes[4];
    if (fread(count_bytes, 1, 4, f) != 4) goto error;
    entry_count = count_bytes[0] | (count_bytes[1] << 8) | (count_bytes[2] << 16) | (count_bytes[3] << 24);

    entries = (GMPakEntry*)malloc(sizeof(GMPakEntry) * entry_count);
    if (!entries) goto error;

    for (uint32_t i = 0; i < entry_count; i++) {
        uint8_t entry_bytes[9];

        if (fread(entries[i].name, 1, 16, f) != 16) break;
        entries[i].name[16] = '\0';
        if (fread(entry_bytes, 1, 9, f) != 9) break;

        entries[i].type = entry_bytes[0];
        entries[i].offset = entry_bytes[1] | (entry_bytes[2] << 8) | (entry_bytes[3] << 16) | (entry_bytes[4] << 24);
        entries[i].size = entry_bytes[5] | (entry_bytes[6] << 8) | (entry_bytes[7] << 16) | (entry_bytes[8] << 24);
        entries[i].cached_data = nullptr;
    }
    return;

error:
    if (entries) free(entries);
    if (f) fclose(f);
    entries = nullptr;
    entry_count = 0;
    f = nullptr;
}

GMPak::~GMPak() {
    clear_cache();
    if (entries) free(entries);
    if (f) fclose(f);
}

void GMPak::clear_cache() {
    if (!entries) return;
    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].cached_data) {
            free(entries[i].cached_data);
            entries[i].cached_data = nullptr;
        }
    }
}

void* GMPak::get_entry(const char* name, uint32_t* out_size, uint8_t* out_type, bool preload) {
    if (!f) return nullptr;

    for (uint32_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
            if (out_size) *out_size = entries[i].size;
            if (out_type) *out_type = entries[i].type;

            if (entries[i].cached_data) {
                return entries[i].cached_data;
            }

            void* data = malloc(entries[i].size);
            if (!data) return nullptr;

            if (fseek(f, entries[i].offset, SEEK_SET) != 0) {
                free(data);
                return nullptr;
            }
            if (fread(data, 1, entries[i].size, f) != entries[i].size) {
                free(data);
                return nullptr;
            }

            if (preload) {
                entries[i].cached_data = data;
            }
            return data;
        }
    }
    return nullptr;
}

void GMPak::preload_entries(const char* prefix, int start, int end) {
    char name[17];
    int total_frames = 0;

    // Find highest index for wrap-aware distance
    for(uint32_t i=0; i<entry_count; i++) {
        if(strncmp(entries[i].name, prefix, strlen(prefix)) == 0) {
            int idx = atoi(entries[i].name + strlen(prefix));
            if(idx > total_frames) total_frames = idx;
        }
    }
    total_frames++; // count

    for (uint32_t i = 0; i < entry_count; i++) {
        if (entries[i].cached_data) {
            if (strncmp(entries[i].name, prefix, strlen(prefix)) == 0) {
                int idx = atoi(entries[i].name + strlen(prefix));
                // Circular distance
                int dist = abs(idx - start);
                if (dist > total_frames / 2) dist = total_frames - dist;

                if (dist > 15) { // Evict if distance > 15 frames
                    free(entries[i].cached_data);
                    entries[i].cached_data = nullptr;
                }
            }
        }
    }

    for (int i = start; i <= end; i++) {
        int idx = (i < 0) ? (i + total_frames) : (i % total_frames);
        sprintf(name, "%s%04d", prefix, idx);
        get_entry(name, nullptr, nullptr, true);
    }
}
