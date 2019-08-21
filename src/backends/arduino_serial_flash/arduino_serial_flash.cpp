#ifdef PHYLUM_ENABLE_SERIAL_FLASH

#include <cinttypes>
#include <cstdarg>
#include <cstdio>

#include "arduino_serial_flash.h"
#include "serial_flash_allocator.h"

using namespace alogging;

namespace phylum {

static inline uint32_t get_sf_address(const Geometry &g, BlockAddress a) {
    return (a.block * g.pages_per_block * g.sectors_per_page * g.sector_size) + a.position;
}

ArduinoSerialFlashBackend::ArduinoSerialFlashBackend(StorageBackendCallbacks &callbacks) : callbacks_(&callbacks) {
}

bool ArduinoSerialFlashBackend::initialize(uint8_t cs, sector_index_t sector_size, uint32_t maximum_blocks) {
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

    if (maximum_blocks > 0 && number_of_blocks > maximum_blocks) {
        sdebug() << "Limited number of blocks to " << maximum_blocks << " from " << number_of_blocks << endl;
        number_of_blocks = maximum_blocks;
    }

    geometry_ = Geometry{ number_of_blocks, pages_per_block, sectors_per_page, sector_size };

    sdebug() << "Initialized: " << geometry_ << " block-size=" << block_size << " capacity=" << capacity << endl;

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

void ArduinoSerialFlashBackend::geometry(Geometry g) {
    geometry_ = g;
}

bool ArduinoSerialFlashBackend::eraseAll() {
    serial_flash_.eraseAll();
    return true;
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
    serial_flash_.read(address, d, n);
    #if PHYLUM_ARDUINO_DEBUG > 1
    sdebug() << "Read(" << addr << " " << n << " " << address << ")" << endl;
    #if PHYLUM_ARDUINO_DEBUG > 5
    auto s = sdebug();
    for (size_t i = 0; i < n; ++i) {
        s.printf("%02x ", ((uint8_t *)d)[i]);
    }
    #endif
    #endif
    return true;
}

bool ArduinoSerialFlashBackend::write(BlockAddress addr, void *d, size_t n) {
    auto address = get_sf_address(geometry_, addr);
    #if PHYLUM_ARDUINO_DEBUG > 1
    sdebug() << "Write(" << addr << " " << n << " " << address << ")" << endl;
    #if PHYLUM_ARDUINO_DEBUG > 5
    auto s = sdebug();
    for (size_t i = 0; i < n; ++i) {
        s.printf("%02x ", ((uint8_t *)d)[i]);
    }
    #endif
    #endif
    serial_flash_.write(address, d, n);
    return true;
}

}

#endif // PHYLUM_ENABLE_SERIAL_FLASH
