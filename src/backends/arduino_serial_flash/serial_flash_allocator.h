#ifndef __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED

#include <phylum/block_alloc.h>

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

    uint32_t number_of_free_blocks();

};

}

#endif // __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
