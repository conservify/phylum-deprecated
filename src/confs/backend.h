#ifndef __CONFS_BACKEND_H_INCLUDED
#define __CONFS_BACKEND_H_INCLUDED

#include <confs/private.h>

namespace confs {

class StorageBackend {
public:
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual Geometry &geometry() = 0;
    virtual size_t size() = 0;
    virtual bool erase(block_index_t block) = 0;
    virtual bool read(confs_sector_addr_t addr, void *d, size_t n) = 0;
    virtual bool write(confs_sector_addr_t addr, void *d, size_t n) = 0;

};

}

#endif
