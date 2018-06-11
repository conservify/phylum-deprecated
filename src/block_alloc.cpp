#include "phylum/block_alloc.h"

namespace phylum {

SequentialBlockAllocator::SequentialBlockAllocator(Geometry &geometry) : geometry_(&geometry) {
}

bool SequentialBlockAllocator::initialize(Geometry &geometry) {
    return true;
}

AllocatorState SequentialBlockAllocator::state() {
    return { block_ };
}

void SequentialBlockAllocator::state(AllocatorState state) {
    block_ = state.head;
}

block_index_t SequentialBlockAllocator::allocate(BlockType type) {
    assert(block_ < geometry_->number_of_blocks);
    return block_++;
}

void SequentialBlockAllocator::free(block_index_t block) {
}

#ifndef ARDUINO

QueueBlockAllocator::QueueBlockAllocator(Geometry &geometry) : geometry_(&geometry) {
}

bool QueueBlockAllocator::initialize(Geometry &geometry) {
    return true;
}

AllocatorState QueueBlockAllocator::state() {
    return { BLOCK_INDEX_INVALID };
}

void QueueBlockAllocator::state(AllocatorState state) {
}

block_index_t QueueBlockAllocator::allocate(BlockType type) {
    if (!initialized_) {
        for (auto i = 3; i < (int32_t)geometry_->number_of_blocks; ++i) {
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

}
