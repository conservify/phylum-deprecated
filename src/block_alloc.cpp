#include "confs/block_alloc.h"

namespace confs {

BlockAllocator::BlockAllocator(StorageBackend &storage) : storage_(&storage) {
}

bool BlockAllocator::initialize(Geometry &geometry) {
    return true;
}

block_index_t BlockAllocator::allocate() {
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
}

void BlockAllocator::free(block_index_t block) {
    free_.push(block);
}

}
