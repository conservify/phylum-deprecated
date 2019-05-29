#ifndef __PHYLUM_BACKEND_H_INCLUDED
#define __PHYLUM_BACKEND_H_INCLUDED

#include <phylum/private.h>

namespace phylum {

class StorageBackend {
public:
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual Geometry &geometry() = 0;
    virtual void geometry(Geometry g) = 0;
    virtual bool erase(block_index_t block) = 0;
    virtual bool read(BlockAddress addr, void *d, size_t n) = 0;
    virtual bool write(BlockAddress addr, void *d, size_t n) = 0;
    virtual bool eraseAll() = 0;

};

}

#endif
