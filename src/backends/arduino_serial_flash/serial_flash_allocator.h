#ifndef __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED

#ifdef ARDUINO

#include <SerialFlash.h>

#include <phylum/block_alloc.h>

#include "arduino_serial_flash.h"

namespace phylum {

class SerialFlashAllocator : public ReusableBlockAllocator {
private:
    static constexpr size_t MapSize = ArduinoSerialFlashBackend::MaximumBlocks / 8;
    uint8_t map_[MapSize]{ 0 };

    ArduinoSerialFlashBackend *storage_;

public:
    SerialFlashAllocator(ArduinoSerialFlashBackend &storage);

public:
    virtual bool initialize();
    virtual block_index_t allocate(BlockType type) override;
    virtual bool free(block_index_t block, block_age_t age) override;

private:
    struct ScanInfo {
        block_index_t block;
        block_age_t age;
    };

    bool scan(bool free_only, ScanInfo &info);

};

}

#endif // ARDUINO

#endif // __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
