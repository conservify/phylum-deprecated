#ifndef __PHYLUM_BLOCK_ALLOC_H_INCLUDED
#define __PHYLUM_BLOCK_ALLOC_H_INCLUDED

#include <phylum/private.h>
#include <phylum/backend.h>

#include <queue>

namespace phylum {

class BlockAllocator {
private:
    bool initialized_{ false };
    std::queue<block_index_t> free_;
    StorageBackend *storage_;

public:
    BlockAllocator(StorageBackend &storage);

public:
    bool initialize(Geometry &geometry);
    block_index_t allocate();
    void free(block_index_t block);

};

}

#endif
