#include <Arduino.h>

#include <cstdarg>
#include <cstdio>

#include "arduino_sd.h"

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
    auto number_of_fs_blocks = number_of_sd_blocks / (g.sectors_per_page * g.pages_per_block);

    geometry_ = g;
    geometry_.number_of_blocks = number_of_sd_blocks;

    sdebug() << "Ready: FsBlocks " << number_of_fs_blocks << " SdBlocks: " << number_of_sd_blocks << std::endl;

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
    sdebug() << "Erase(" << block << ")" << std::endl;
    return sd_raw_erase(&sd_, first_sd_block, last_sd_block);
}

bool ArduinoSdBackend::read(BlockAddress addr, void *d, size_t n) {
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    sdebug() << "Read(" << addr << " " << n << ")" << std::endl;
    return sd_raw_read_data(&sd_, sd_block, offset, n, (uint8_t *)d);
}

bool ArduinoSdBackend::write(BlockAddress addr, void *d, size_t n) {
    auto sd_block = get_sd_block(geometry_, addr);
    auto offset = addr.sector_offset(geometry_);
    sdebug() << "Write(" << addr << " " << n << ")" << std::endl;
    return sd_raw_write_data(&sd_, sd_block, offset, n, (uint8_t *)d, true);
}

}
