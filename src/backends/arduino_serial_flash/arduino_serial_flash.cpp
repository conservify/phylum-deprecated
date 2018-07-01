#ifdef ARDUINO

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "arduino_serial_flash.h"

namespace phylum {

static inline uint32_t get_sf_address(const Geometry &g, BlockAddress a) {
    return (a.block * g.pages_per_block * g.sectors_per_page * SectorSize) + a.position;
}

ArduinoSerialFlashBackend::ArduinoSerialFlashBackend() {
}

bool ArduinoSerialFlashBackend::initialize(uint8_t cs) {
    if (!serial_flash_.begin(cs)) {
        return false;
    }

    unsigned char id[5];
    SerialFlash.readID(id);

    auto capacity = serial_flash_.capacity(id);
    auto block_size = serial_flash_.blockSize();
    if (capacity == 0 || block_size == 0) {
        return false;
    }

    auto sectors_per_page = (page_index_t)4;
    auto pages_per_block = (page_index_t)(block_size / (sectors_per_page * SectorSize));
    auto number_of_blocks = (block_index_t)(capacity / block_size);

    geometry_ = Geometry{ number_of_blocks, pages_per_block, sectors_per_page, SectorSize };

    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Initialized: " << geometry_ << endl;
    #endif

    if (false) {
        serial_flash_.eraseAll();
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

#endif // ARDUINO
