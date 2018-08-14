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
    StorageBackend *storage_;
    uint8_t map_[MapSize]{ 0 };

public:
    SerialFlashAllocator(StorageBackend &storage);

public:
    virtual bool initialize();
    virtual block_index_t allocate(BlockType type) override;
    virtual bool free(block_index_t block, block_age_t age) override;

public:
    struct ScanInfo {
        block_index_t block;
        block_age_t age;
    };

    bool scan(bool free_only, ScanInfo &info);

    bool is_taken(block_index_t block, BlockHead &header);

    bool is_taken(block_index_t block);

    uint32_t number_of_free_blocks();

    bool free_all_blocks();

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
