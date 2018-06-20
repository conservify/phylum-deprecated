#ifndef __PHYLUM_BACKEND_H_INCLUDED
#define __PHYLUM_BACKEND_H_INCLUDED

#include <phylum/private.h>

namespace phylum {

class StorageBackend {
public:
    virtual bool open() = 0;
    virtual bool close() = 0;
    virtual Geometry &geometry() = 0;
    virtual bool erase(block_index_t block) = 0;
    virtual bool read(BlockAddress addr, void *d, size_t n) = 0;
    virtual bool write(BlockAddress addr, void *d, size_t n) = 0;

};

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

    virtual bool erase(block_index_t block) override {
        return target.erase(block);
    }

    virtual bool read(BlockAddress addr, void *d, size_t n) override;

    virtual bool write(BlockAddress addr, void *d, size_t n) override;

};

}

#endif
