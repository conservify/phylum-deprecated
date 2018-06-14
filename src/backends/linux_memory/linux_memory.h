#ifndef __PHYLUM_LINUX_MEMORY_H_INCLUDED
#define __PHYLUM_LINUX_MEMORY_H_INCLUDED

#include <phylum/phylum.h>
#include <phylum/private.h>
#include <phylum/backend.h>

#include "debug_log.h"

namespace phylum {

class LinuxMemoryBackend : public StorageBackend {
private:
    StorageLog log_;
    Geometry geometry_;
    uint64_t size_;
    uint8_t *ptr_;

public:
    static uint8_t EraseByte;

public:
    LinuxMemoryBackend();

public:
    StorageLog &log() {
        return log_;
    }

public:
    bool initialize(Geometry geometry);

public:
    bool open() override;
    bool close() override;
    Geometry &geometry() override;
    bool erase(block_index_t block) override;
    void randomize();
    bool read(BlockAddress addr, void *d, size_t n) override;
    bool write(BlockAddress addr, void *d, size_t n) override;

};

}

#endif
