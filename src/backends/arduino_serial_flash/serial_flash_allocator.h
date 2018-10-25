#ifndef __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED

#include "phylum/private.h"
#include "phylum/block_alloc.h"
#include "phylum/visitor.h"

#include "arduino_serial_flash.h"

namespace phylum {

class SerialFlashAllocator : public ReusableBlockAllocator {
public:
    static constexpr size_t MaximumBlocks = 64;
    static constexpr size_t MapSize = MaximumBlocks / 8;

private:
    static constexpr int32_t PreallocationSize = 8;
    uint32_t preallocated_[PreallocationSize];
    StorageBackend *storage_;
    uint8_t map_[MapSize]{ 0 };

public:
    SerialFlashAllocator(StorageBackend &storage);

public:
    bool initialize();
    AllocatedBlock allocate(BlockType type) override;
    bool free(block_index_t block, block_age_t age) override;

private:
    AllocatedBlock allocate_internal(BlockType type);

public:
    struct ScanInfo {
        block_index_t block;
        block_age_t age;
    };

    bool scan(bool free_only, ScanInfo &selected);

    bool scan(bool free_only, ScanInfo *blocks, size_t size);

    bool is_taken(block_index_t block, BlockHead &header);

    bool is_taken(block_index_t block);

    uint32_t number_of_free_blocks();

    bool free_all_blocks();

    bool preallocate(uint32_t expected_size) override;

};

class TakenBlockTracker : public BlockVisitor {
private:
    uint8_t map_[SerialFlashAllocator::MapSize]{ 0 };

public:
    TakenBlockTracker();

public:
    void block(block_index_t block) override;
    bool is_free(block_index_t block);

};

}

#endif // __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
