#include "gmpak.hpp"
#include <cstring>

GMPak::GMPak(const std::string& filename) : f(nullptr) {
    f = fopen(filename.c_str(), "rb");
    if (!f) return;

    uint8_t header[10];
    if (fread(header, 1, 10, f) != 10) goto error;
    if (memcmp(header, "GMPK", 4) != 0) goto error;
    // Little-endian parsing
    uint32_t toc_off = header[6] | (header[7] << 8) | (header[8] << 16) | (header[9] << 24);

    if (fseek(f, toc_off, SEEK_SET) != 0) goto error;

    uint32_t count;
    uint8_t count_bytes[4];
    if (fread(count_bytes, 1, 4, f) != 4) goto error;
    count = count_bytes[0] | (count_bytes[1] << 8) | (count_bytes[2] << 16) | (count_bytes[3] << 24);

    for (uint32_t i = 0; i < count; i++) {
        char name[17];
        memset(name, 0, 17);
        uint8_t type;
        uint32_t off, size;
        uint8_t entry_bytes[9];

        if (fread(name, 1, 16, f) != 16) break;
        if (fread(entry_bytes, 1, 9, f) != 9) break;

        type = entry_bytes[0];
        off = entry_bytes[1] | (entry_bytes[2] << 8) | (entry_bytes[3] << 16) | (entry_bytes[4] << 24);
        size = entry_bytes[5] | (entry_bytes[6] << 8) | (entry_bytes[7] << 16) | (entry_bytes[8] << 24);

        std::string name_str(name);
        toc[name_str] = {type, off, size};
    }
    return;

error:
    fclose(f);
    f = nullptr;
}

GMPak::~GMPak() {
    if (f) fclose(f);
}

std::vector<uint8_t> GMPak::get_entry(const std::string& name) {
    if (!f || toc.find(name) == toc.end()) return {};

    const auto& entry = toc[name];
    std::vector<uint8_t> data(entry.size);
    if (fseek(f, entry.offset, SEEK_SET) != 0) return {};
    if (fread(data.data(), 1, entry.size, f) != entry.size) return {};

    return data;
}
