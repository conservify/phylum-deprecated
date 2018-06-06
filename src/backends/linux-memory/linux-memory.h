#ifndef __CONFS_LINUX_MEMORY_H_INCLUDED
#define __CONFS_LINUX_MEMORY_H_INCLUDED

#include <confs/confs.h>
#include <confs/private.h>
#include <confs/backend.h>

#include "debug_log.h"

namespace confs {

class LinuxMemoryBackend : public StorageBackend {
private:
    StorageLog log_;
    Geometry geometry_;
    size_t size_;
    uint8_t *ptr_;

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
    size_t size() override;
    bool erase(block_index_t block) override;
    bool read(SectorAddress addr, size_t offset, void *d, size_t n) override;
    bool write(SectorAddress addr, size_t offset, void *d, size_t n) override;
    void randomize();

};

}

#endif
