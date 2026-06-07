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

    // Little Endian
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
    if (entries) free(entries);
    if (f) fclose(f);
}

void* GMPak::get_entry(const char* name, uint32_t* out_size, uint8_t* out_type) {
    if (!f) return nullptr;

    for (uint32_t i = 0; i < entry_count; i++) {
        if (strcmp(entries[i].name, name) == 0) {
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

            if (out_size) *out_size = entries[i].size;
            if (out_type) *out_type = entries[i].type;
            return data;
        }
    }
    return nullptr;
}
