#ifndef __PHYLUM_CACHING_STORAGE_H_INCLUDED
#define __PHYLUM_CACHING_STORAGE_H_INCLUDED

#include "backend.h"

namespace phylum {

class SectorCachingStorage : public StorageBackend {
private:
    StorageBackend &target;
    SectorAddress sector_;
    uint8_t buffer_[SectorSize];

public:
    SectorCachingStorage(StorageBackend &target);

public:
    virtual bool open() override {
        return target.open();
    }

    virtual bool close() override {
        return target.close();
    }

    virtual Geometry &geometry() override {
        return target.geometry();
    }

    virtual void geometry(Geometry g) override {
        target.geometry(g);
    }

    virtual bool erase(block_index_t block) override {
        return target.erase(block);
    }

    virtual bool read(BlockAddress addr, void *d, size_t n) override;

    virtual bool write(BlockAddress addr, void *d, size_t n) override;

};

}

#endif
