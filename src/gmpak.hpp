#ifndef GMPAK_HPP
#define GMPAK_HPP

#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <stdint.h>

struct GMPakEntry {
    uint8_t type;
    uint32_t offset;
    uint32_t size;
};

class GMPak {
public:
    GMPak(const std::string& filename);
    ~GMPak();

    bool is_open() const { return f != nullptr; }
    std::vector<uint8_t> get_entry(const std::string& name);

    std::map<std::string, GMPakEntry> toc;

private:
    FILE *f;
};

#endif // GMPAK_HPP
