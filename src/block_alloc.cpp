#include "phylum/block_alloc.h"

namespace phylum {

EmptyAllocator empty_allocator;

SequentialBlockAllocator::SequentialBlockAllocator() {
}

bool SequentialBlockAllocator::initialize(Geometry &geometry) {
    geometry_ = &geometry;

    return true;
}

AllocatorState SequentialBlockAllocator::state() {
    return { block_ };
}

void SequentialBlockAllocator::state(AllocatorState state) {
    block_ = state.head;
}

AllocatedBlock SequentialBlockAllocator::allocate(BlockType type) {
    assert(geometry_ != nullptr);
    assert(block_ < geometry_->number_of_blocks);
    return { block_++, 0, false };
}

bool SequentialBlockAllocator::free(block_index_t block, block_age_t age) {
    return true;
}

#ifndef ARDUINO

DebuggingBlockAllocator::DebuggingBlockAllocator() {
}

AllocatedBlock DebuggingBlockAllocator::allocate(BlockType type) {
    auto alloc = SequentialBlockAllocator::allocate(type);
    assert(allocations_.find(alloc.block) == allocations_.end());
    allocations_[alloc.block] = type;
    return alloc;
}

QueueBlockAllocator::QueueBlockAllocator() {
}

bool QueueBlockAllocator::initialize(Geometry &geometry) {
    geometry_ = &geometry;

    return true;
}

AllocatorState QueueBlockAllocator::state() {
    return { BLOCK_INDEX_INVALID };
}

void QueueBlockAllocator::state(AllocatorState state) {
}

AllocatedBlock QueueBlockAllocator::allocate(BlockType type) {
    assert(geometry_ != nullptr);

    if (!initialized_) {
        for (auto i = 3; i < (int32_t)geometry_->number_of_blocks; ++i) {
            free(i, 0);
        }
        initialized_ = true;
    }

    assert(!free_.empty());

    auto block = free_.front();

    free_.pop();

    return { block, 0, false };
}

bool QueueBlockAllocator::free(block_index_t block, block_age_t age) {
    free_.push(block);

    return true;
}

#endif

}
