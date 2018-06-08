#include "phylum/block_alloc.h"

namespace phylum {

BlockAllocator::BlockAllocator(StorageBackend &storage) : storage_(&storage) {
}

bool BlockAllocator::initialize(Geometry &geometry) {
    return true;
}

block_index_t BlockAllocator::allocate() {
    #ifndef ARDUINO
    if (!initialized_) {
        for (auto i = 3; i < (int32_t)storage_->geometry().number_of_blocks; ++i) {
            free(i);
        }
        initialized_ = true;
    }

    assert(!free_.empty());

    auto block = free_.front();

    free_.pop();

    return block;
    #else
    return 0;
    #endif
}

void BlockAllocator::free(block_index_t block) {
    #ifndef ARDUINO
    free_.push(block);
    #endif
}

}
