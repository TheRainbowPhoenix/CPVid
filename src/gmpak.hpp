#ifndef GMPAK_HPP
#define GMPAK_HPP

#include <stdint.h>
#include <stdio.h>

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
    void* get_entry(const char* name, uint32_t* size, uint8_t* type);
    void preload_entries(const char* prefix, int start, int end);

    uint32_t entry_count;
    GMPakEntry* entries;

private:
    FILE* f;
    uint32_t last_index;
};

#endif
