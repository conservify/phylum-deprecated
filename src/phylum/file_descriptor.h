#ifndef __PHYLUM_FILE_DESCRIPTOR_H_INCLUDED
#define __PHYLUM_FILE_DESCRIPTOR_H_INCLUDED

#include <cinttypes>
#include <cstring>

namespace phylum {

struct FileDescriptor {
    char name[16];
    uint64_t maximum_size;

    FileDescriptor() : name{ 0 }, maximum_size{ 0 } {
    }

    FileDescriptor(const char *name, uint64_t maximum_size) : maximum_size(maximum_size) {
        strncpy(this->name, name, sizeof(this->name));
        this->name[sizeof(this->name) - 1] = 0;
    }
};

enum class OpenMode {
    Read,
    Write
};

}

#endif
