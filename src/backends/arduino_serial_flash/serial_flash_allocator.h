#ifndef __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED

#ifdef ARDUINO

#include <SerialFlash.h>

#include <phylum/block_alloc.h>

#include "arduino_serial_flash.h"

namespace phylum {

class SerialFlashAllocator : public BlockManager {
private:
    static constexpr size_t MapSize = 64 / 8;
    uint8_t map_[MapSize]{ 0 };

    ArduinoSerialFlashBackend *storage_;

public:
    SerialFlashAllocator(ArduinoSerialFlashBackend &storage);

public:
    virtual block_index_t allocate(BlockType type) override;
    virtual bool initialize(Geometry &geometry) override;
    virtual AllocatorState state() override;
    virtual void state(AllocatorState state) override;
    virtual void free(block_index_t block) override;

};

}

#endif // ARDUINO

#endif // __PHYLUM_SERIAL_FLASH_ALLOCATOR_H_INCLUDED