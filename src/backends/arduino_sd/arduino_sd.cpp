#ifdef PHYLUM_ENABLE_SD
#ifdef ARDUINO

#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "arduino_sd.h"

using namespace alogging;

namespace phylum {

static inline uint32_t get_sd_block(const Geometry &g, BlockAddress a) {
    return (a.block * g.pages_per_block * g.sectors_per_page) + (a.sector_number(g));
}

ArduinoSdBackend::ArduinoSdBackend() {
}

bool ArduinoSdBackend::initialize(const Geometry &g, uint8_t cs) {
    if (!sd_raw_initialize(&sd_, cs)) {
        return false;
    }

    auto number_of_sd_blocks = sd_raw_card_size(&sd_);
    geometry_ = Geometry::from_physical_block_layout(number_of_sd_blocks, SectorSize);

    sdebug() << "Ready: " << geometry_ << endl;

    return true;
}

bool ArduinoSdBackend::open() {
    return true;
}

bool ArduinoSdBackend::close() {
    return true;
}

Geometry &ArduinoSdBackend::geometry() {
    return geometry_;
}

void ArduinoSdBackend::geometry(Geometry g) {
    geometry_ = g;
}

bool ArduinoSdBackend::eraseAll() {
    return false;
}

bool ArduinoSdBackend::erase(block_index_t block) {
    #if defined(PHYLUM_READ_ONLY)
    assert(false);
    return true;
    #else
    auto first_sd_block = get_sd_block(geometry_, BlockAddress{ block, 0 });
    auto last_sd_block = get_sd_block(geometry_, BlockAddress{ block + 1, 0 });
    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Erase(" << block << ")" << endl;
    #endif
    if (!sd_raw_erase(&sd_, first_sd_block, last_sd_block)) {
        phylog().errors() << "Error erasing: block=" << block << endl;
        return false;
    }
    return true;
    #endif
}

bool ArduinoSdBackend::read(BlockAddress addr, void *d, size_t n) {
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Read(" << addr << " " << n << ")" << endl;
    #endif
    if (!sd_raw_read_data(&sd_, sd_block, offset, n, (uint8_t *)d)) {
        phylog().errors() << "Error reading: " << addr << " bytes=" << n << endl;
        return false;
    }
    return true;
}

bool ArduinoSdBackend::write(BlockAddress addr, void *d, size_t n) {
    #if defined(PHYLUM_READ_ONLY)
    assert(false);
    return true;
    #else
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Write(" << addr << " " << n << ")" << endl;
    #endif
    if (!sd_raw_write_data(&sd_, sd_block, offset, n, (uint8_t *)d, true)) {
        phylog().errors() << "Error writing: " << addr << " bytes=" << n << endl;
        return false;
    }
    return true;
    #endif
}

}

#endif // ARDUINO
#endif // PHYLUM_ENABLE_SD
