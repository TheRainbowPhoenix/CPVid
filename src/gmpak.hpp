#ifndef GMPAK_HPP
#define GMPAK_HPP

#include <stdint.h>
#include <cstdio>

struct GMPakEntry {
    char name[17];
    uint8_t type;
    uint32_t offset;
    uint32_t size;
    void* cached_data;
};

class GMPak {
public:
    GMPak(const char* filename);
    ~GMPak();

    bool is_open() const { return f != nullptr; }

    // Returns pointer to data. If cached, returns cached data.
    // If not cached, returns malloc'd buffer (caller must free).
    // If preload is true, it will be cached.
    void* get_entry(const char* name, uint32_t* out_size, uint8_t* out_type, bool preload = false);

    void clear_cache();
    void preload_entries(const char* prefix, int start, int end);

    GMPakEntry* entries;
    uint32_t entry_count;

private:
    FILE *f;
};

#endif // GMPAK_HPP
