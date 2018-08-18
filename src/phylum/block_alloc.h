#ifndef __PHYLUM_BLOCK_ALLOC_H_INCLUDED
#define __PHYLUM_BLOCK_ALLOC_H_INCLUDED

#include <phylum/private.h>
#include <phylum/backend.h>

#ifndef ARDUINO
#include <queue>
#include <map>
#include <set>
#endif

namespace phylum {

// TODO: This should be customizable per-allocator. Template overhead doesn't
// seem worth the effort though.
struct AllocatorState {
    block_index_t head;

    AllocatorState(block_index_t head = BLOCK_INDEX_INVALID) : head(head) {
    }
};

struct AllocatedBlock {
    block_index_t block{ BLOCK_INDEX_INVALID };
    block_age_t age{ 0 };
    bool erased{ false };

    AllocatedBlock() {
    }

    AllocatedBlock(block_index_t block, bool erased) : block(block), erased(erased) {
    }

    bool valid() {
        return is_valid_block(block);
    }
};

class BlockAllocator {
public:
    virtual AllocatedBlock allocate(BlockType type) = 0;
};

class ReusableBlockAllocator : public BlockAllocator {
public:
    virtual bool free(block_index_t block, block_age_t age) = 0;
};

class EmptyAllocator : public BlockAllocator {
public:
    AllocatedBlock allocate(BlockType type) override {
        return { BLOCK_INDEX_INVALID, false };
    }
};

class BlockManager : public ReusableBlockAllocator {
public:
    virtual bool initialize(Geometry &geometry) = 0;
    virtual AllocatorState state() = 0;
    virtual void state(AllocatorState state) = 0;

};

class SequentialBlockAllocator : public BlockManager {
private:
    Geometry *geometry_{ nullptr };
    uint32_t block_{ 3 };

public:
    SequentialBlockAllocator();

public:
    bool initialize(Geometry &geometry) override;
    bool free(block_index_t block, block_age_t age) override;
    AllocatorState state() override;
    void state(AllocatorState state) override;
    AllocatedBlock allocate(BlockType type) override;

};

#ifndef ARDUINO

class DebuggingBlockAllocator : public SequentialBlockAllocator {
private:
    std::map<block_index_t, BlockType> allocations_;

public:
    DebuggingBlockAllocator();

public:
    std::map<block_index_t, BlockType> &allocations() {
        return allocations_;
    }

    std::set<block_index_t> blocks_of_type(BlockType type) {
        std::set<block_index_t> blocks;
        for (auto &pair : allocations_) {
            if (pair.second == type) {
                blocks.insert(pair.first);
            }
        }
        return blocks;
    }

public:
    AllocatedBlock allocate(BlockType type) override;

};

class QueueBlockAllocator : public BlockManager {
private:
    Geometry *geometry_{ nullptr };
    bool initialized_{ false };
    std::queue<block_index_t> free_;

public:
    QueueBlockAllocator();

public:
    bool initialize(Geometry &geometry) override;
    AllocatorState state() override;
    void state(AllocatorState state) override;
    AllocatedBlock allocate(BlockType type) override;
    bool free(block_index_t block, block_age_t age) override;

};
#endif

extern EmptyAllocator empty_allocator;

}

#endif
