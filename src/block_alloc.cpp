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

block_index_t SequentialBlockAllocator::allocate(BlockType type) {
    assert(geometry_ != nullptr);
    assert(block_ < geometry_->number_of_blocks);
    return block_++;
}

void SequentialBlockAllocator::free(block_index_t block, block_age_t age) {
}

#ifndef ARDUINO

DebuggingBlockAllocator::DebuggingBlockAllocator() {
}

block_index_t DebuggingBlockAllocator::allocate(BlockType type) {
    auto block = SequentialBlockAllocator::allocate(type);
    allocations_[block] = type;
    return block;
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

block_index_t QueueBlockAllocator::allocate(BlockType type) {
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

    return block;
}

void QueueBlockAllocator::free(block_index_t block, block_age_t age) {
    free_.push(block);
}

#endif

}
