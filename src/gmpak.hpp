#ifndef GMPAK_HPP
#define GMPAK_HPP

#include <stdint.h>
#include <cstdio>

struct GMPakEntry {
    char name[17];
    uint8_t type;
    uint32_t offset;
    uint32_t size;
};

class GMPak {
public:
    GMPak(const char* filename);
    ~GMPak();

    bool is_open() const { return f != nullptr; }

    // Returns malloc'd buffer. Caller must free.
    void* get_entry(const char* name, uint32_t* out_size, uint8_t* out_type);

    GMPakEntry* entries;
    uint32_t entry_count;

private:
    FILE *f;
};

#endif // GMPAK_HPP
