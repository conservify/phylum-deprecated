#ifndef __PHYLUM_FILE_DESCRIPTOR_H_INCLUDED
#define __PHYLUM_FILE_DESCRIPTOR_H_INCLUDED

#include <cinttypes>

namespace phylum {

enum class WriteStrategy {
    Append,
    Rolling
};

struct FileDescriptor {
    char name[16];
    WriteStrategy strategy;
    uint64_t maximum_size;
};

enum class OpenMode {
    Read,
    Write
};

}

#endif
