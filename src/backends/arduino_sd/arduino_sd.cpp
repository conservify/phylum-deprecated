#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "arduino_sd.h"

namespace phylum {

static inline uint32_t get_sd_block(const Geometry &g, BlockAddress a) {
    return (a.block * g.pages_per_block * g.sectors_per_page) + (a.sector_number(g));
}

static void debugf(char const *f, ...) {
    char buffer[256];
    va_list args;
    va_start(args, f);
    vsnprintf(buffer, sizeof(buffer), f, args);
    Serial.print("phylum: ");
    Serial.println(buffer);
    va_end(args);
}

ArduinoSdBackend::ArduinoSdBackend() {
}

bool ArduinoSdBackend::initialize(const Geometry &g, uint8_t cs) {
    if (!sd_raw_initialize(&sd_, cs)) {
        return false;
    }

    auto number_of_sd_blocks = sd_raw_card_size(&sd_);
    auto number_of_fs_blocks = number_of_sd_blocks / (g.sectors_per_page * g.pages_per_block);

    geometry_ = g;
    geometry_.number_of_blocks = number_of_sd_blocks;

    debugf("FS Blocks: %d SD Blocks: %d", number_of_fs_blocks, number_of_sd_blocks);

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

bool ArduinoSdBackend::erase(block_index_t block) {
    auto first_sd_block = get_sd_block(geometry_, BlockAddress{ block, 0 });
    auto last_sd_block = get_sd_block(geometry_, BlockAddress{ block + 1, 0 });
    debugf("Erase: block(%d) sd(%d - %d)", block, first_sd_block, last_sd_block);
    return sd_raw_erase(&sd_, first_sd_block, last_sd_block);
}

bool ArduinoSdBackend::read(SectorAddress addr, size_t offset, void *d, size_t n) {
    return read(BlockAddress{ addr, offset }, d, n);
}

bool ArduinoSdBackend::write(SectorAddress addr, size_t offset, void *d, size_t n) {
    return write(BlockAddress{ addr, offset }, d, n);
}

bool ArduinoSdBackend::read(BlockAddress addr, void *d, size_t n) {
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    debugf("Read: %d+%d (%d)", sd_block, offset, n);
    return sd_raw_read_data(&sd_, sd_block, offset, n, (uint8_t *)d);
}

bool ArduinoSdBackend::write(BlockAddress addr, void *d, size_t n) {
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    debugf("Write: %d+%d (%d)", sd_block, offset, n);
    return sd_raw_write_data(&sd_, sd_block, offset, n, (uint8_t *)d, true);
}

}
