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
    confs_geometry_t geometry_;
    size_t size_;
    uint8_t *ptr_;

public:
    LinuxMemoryBackend();

public:
    StorageLog &log() {
        return log_;
    }

public:
    bool initialize(confs_geometry_t geometry);

public:
    bool open() override;
    bool close() override;
    confs_geometry_t &geometry() override;
    size_t size() override;
    bool erase(block_index_t block) override;
    bool read(confs_sector_addr_t addr, void *d, size_t n) override;
    bool write(confs_sector_addr_t addr, void *d, size_t n) override;

};

}

#endif
