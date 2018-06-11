#include "phylum/block_alloc.h"

namespace phylum {

#ifndef ARDUINO

QueueBlockAllocator::QueueBlockAllocator(StorageBackend &storage) : storage_(&storage) {
}

bool QueueBlockAllocator::initialize(Geometry &geometry) {
    return true;
}

block_index_t QueueBlockAllocator::allocate(BlockType type) {
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

void QueueBlockAllocator::free(block_index_t block) {
    free_.push(block);
}

#endif

SequentialBlockAllocator::SequentialBlockAllocator(StorageBackend &storage) : storage_(&storage) {
}

bool SequentialBlockAllocator::initialize(Geometry &geometry) {
    return true;
}

block_index_t SequentialBlockAllocator::allocate(BlockType type) {
    assert(block_ < storage_->geometry().number_of_blocks);
    return block_++;
}

void SequentialBlockAllocator::free(block_index_t block) {
}

}
