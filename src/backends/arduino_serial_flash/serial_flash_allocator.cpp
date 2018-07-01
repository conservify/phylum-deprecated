#include "serial_flash_allocator.h"

#ifdef ARDUINO

namespace phylum {

SerialFlashAllocator::SerialFlashAllocator(ArduinoSerialFlashBackend &storage) : storage_(&storage) {
}

static inline bool is_block_free(uint8_t *map, block_index_t block) {
    return !(map[block / 8] & (1 << (block % 8)));
}

static inline void set_block_free(uint8_t *map, block_index_t block) {
    map[block / 8] &= ~(1 << (block % 8));
}

static inline void set_block_taken(uint8_t *map, block_index_t block) {
    map[block / 8] |= (1 << (block % 8));
}

block_index_t SerialFlashAllocator::allocate(BlockType type) {
    ScanInfo info;

    if (!scan(true, info)) {
        sdebug() << "Failed to allocate! (" << type << ")" << endl;
        return BLOCK_INDEX_INVALID;
    }

    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Allocate: " << type << " = " << (uint32_t)info.block << endl;
    #endif

    set_block_taken(map_, info.block);

    return info.block;
}

bool SerialFlashAllocator::initialize() {
    ScanInfo info;

    return scan(false, info);
}

bool SerialFlashAllocator::scan(bool free_only, ScanInfo &info) {
    info.block = BLOCK_INDEX_INVALID;
    info.age = BLOCK_AGE_INVALID;

    for (auto block = 3; block < storage_->geometry().number_of_blocks; ++block) {
        if (free_only) {
            if (!is_block_free(map_, block)) {
                continue;
            }
        }

        BlockHead header;
        if (!storage_->read({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
            return false;
        }

        auto valid = header.valid();
        auto taken = valid && header.type != BlockType::Unallocated;

        if (valid) {
            if (header.age < info.age || info.age == BLOCK_AGE_INVALID) {
                info.block = block;
                info.age = header.age;
            }
        }
        else {
            if (info.age != 0) {
                info.block = block;
                info.age = 0;
            }
        }

        if (taken) {
            set_block_taken(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is taken." << endl;
            #endif
        }
        else {
            set_block_free(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is free. (age=" << header.age << ")" << endl;
            #endif
        }
    }

    // These are always taken, anchor blocks and we skip block 0, for now.
    set_block_taken(map_, 0);
    set_block_taken(map_, 1);
    set_block_taken(map_, 2);

    return true;
}

void SerialFlashAllocator::free(block_index_t block, block_age_t age) {
    if (!storage_->erase(block)) {
        sdebug() << "Erase failed! (" << (uint32_t)block << ")" << endl;
    }

    BlockHead header;
    header.fill();
    header.age = age;
    header.type = BlockType::Unallocated;
    if (!storage_->write({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
        sdebug() << "Write erased block failed! (" << (uint32_t)block << ")" << endl;
    }

    set_block_free(map_, block);
}

}

#endif // ARDUINO
