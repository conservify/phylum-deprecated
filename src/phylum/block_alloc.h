#ifndef __PHYLUM_BLOCK_ALLOC_H_INCLUDED
#define __PHYLUM_BLOCK_ALLOC_H_INCLUDED

#include <phylum/private.h>
#include <phylum/backend.h>

#ifndef ARDUINO
#include <queue>
#endif

namespace phylum {

class BlockAllocator {
public:
    virtual bool initialize(Geometry &geometry) = 0;
    virtual block_index_t allocate() = 0;
    virtual void free(block_index_t block) = 0;

};

#ifndef ARDUINO
class QueueBlockAllocator : public BlockAllocator {
private:
    StorageBackend *storage_;
    bool initialized_{ false };
    std::queue<block_index_t> free_;

public:
    QueueBlockAllocator(StorageBackend &storage);

public:
    virtual bool initialize(Geometry &geometry) override;
    virtual block_index_t allocate() override;
    virtual void free(block_index_t block) override;

};
#endif

class SequentialBlockAllocator : public BlockAllocator {
private:
    StorageBackend *storage_;
    uint32_t block_{ 3 };

public:
    SequentialBlockAllocator(StorageBackend &storage);

public:
    virtual bool initialize(Geometry &geometry) override;
    virtual block_index_t allocate() override;
    virtual void free(block_index_t block) override;

};

}

#endif
