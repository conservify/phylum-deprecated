#ifdef PHYLUM_ENABLE_SERIAL_FLASH

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "arduino_serial_flash.h"
#include "serial_flash_allocator.h"

namespace phylum {

static inline uint32_t get_sf_address(const Geometry &g, BlockAddress a) {
    return (a.block * g.pages_per_block * g.sectors_per_page * g.sector_size) + a.position;
}

ArduinoSerialFlashBackend::ArduinoSerialFlashBackend(StorageBackendCallbacks &callbacks) : callbacks_(&callbacks) {
}

bool ArduinoSerialFlashBackend::initialize(uint8_t cs, sector_index_t sector_size) {
    if (!serial_flash_.begin(cs)) {
        return false;
    }

    unsigned char id[5];
    serial_flash_.readID(id);

    auto capacity = serial_flash_.capacity(id);
    auto block_size = serial_flash_.blockSize();
    if (capacity == 0 || block_size == 0) {
        return false;
    }

    auto sectors_per_page = (page_index_t)4;
    auto pages_per_block = (page_index_t)(block_size / (sectors_per_page * sector_size));
    auto number_of_blocks = (block_index_t)(capacity / block_size);

    if (number_of_blocks > SerialFlashAllocator::MaximumBlocks) {
        sdebug() << "Limited number of blocks to " << SerialFlashAllocator::MaximumBlocks << " from " << number_of_blocks << endl;
        number_of_blocks = SerialFlashAllocator::MaximumBlocks;
    }

    geometry_ = Geometry{ number_of_blocks, pages_per_block, sectors_per_page, sector_size };

    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Initialized: " << geometry_ << endl;
    #endif

    return true;
}

bool ArduinoSerialFlashBackend::erase() {
    serial_flash_.eraseAll();

    auto started = millis();
    auto tick = millis();
    while (!serial_flash_.ready()) {
        if (millis() - tick > 1000) {
            if (!callbacks_->busy(millis() - started)) {
                return false;
            }
            tick = millis();
        }
    }

    return true;
}

bool ArduinoSerialFlashBackend::open() {
    return true;
}

bool ArduinoSerialFlashBackend::close() {
    return true;
}

Geometry &ArduinoSerialFlashBackend::geometry() {
    return geometry_;
}

bool ArduinoSerialFlashBackend::erase(block_index_t block) {
    auto address = get_sf_address(geometry_, BlockAddress{ block, 0 });
    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Erase(" << block << " " << address << ")" << endl;
    #endif
    serial_flash_.eraseBlock(address);
    callbacks_->busy(0);
    return true;
}

bool ArduinoSerialFlashBackend::read(BlockAddress addr, void *d, size_t n) {
    auto address = get_sf_address(geometry_, addr);
    #if PHYLUM_ARDUINO_DEBUG > 1
    sdebug() << "Read(" << addr << " " << n << " " << address << ")" << endl;
    #endif
    serial_flash_.read(address, d, n);
    return true;
}

bool ArduinoSerialFlashBackend::write(BlockAddress addr, void *d, size_t n) {
    auto address = get_sf_address(geometry_, addr);
    #if PHYLUM_ARDUINO_DEBUG > 1
    sdebug() << "Write(" << addr << " " << n << " " << address << ")" << endl;
    #endif
    serial_flash_.write(address, d, n);
    return true;
}

}

#endif // PHYLUM_ENABLE_SERIAL_FLASH
