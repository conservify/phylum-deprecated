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

class BlockAllocator {
public:
    virtual bool initialize(Geometry &geometry) = 0;
    virtual AllocatorState state() = 0;
    virtual void state(AllocatorState state) = 0;
    virtual block_index_t allocate(BlockType type) = 0;
    virtual void free(block_index_t block) = 0;

};

class SequentialBlockAllocator : public BlockAllocator {
private:
    Geometry *geometry_{ nullptr };
    uint32_t block_{ 3 };

public:
    SequentialBlockAllocator();

public:
    virtual bool initialize(Geometry &geometry) override;
    virtual AllocatorState state() override;
    virtual void state(AllocatorState state) override;
    virtual block_index_t allocate(BlockType type) override;
    virtual void free(block_index_t block) override;

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
    virtual block_index_t allocate(BlockType type) override;

};

class QueueBlockAllocator : public BlockAllocator {
private:
    Geometry *geometry_{ nullptr };
    bool initialized_{ false };
    std::queue<block_index_t> free_;

public:
    QueueBlockAllocator();

public:
    virtual bool initialize(Geometry &geometry) override;
    virtual AllocatorState state() override;
    virtual void state(AllocatorState state) override;
    virtual block_index_t allocate(BlockType type) override;
    virtual void free(block_index_t block) override;

};
#endif

}

#endif
