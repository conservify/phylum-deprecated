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
    for (auto block = 0; block < storage_->geometry().number_of_blocks; ++block) {
        if (is_block_free(map_, block)) {
            set_block_taken(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Allocate: " << type << " = " << (uint32_t)block << endl;
            #endif
            return block;
        }
    }

    sdebug() << "Failed to allocate! (" << type << ")" << endl;

    return BLOCK_INDEX_INVALID;
}

bool SerialFlashAllocator::initialize() {
    memset(map_, 0, sizeof(map_));

    for (auto block = 3; block < storage_->geometry().number_of_blocks; ++block) {
        BlockHead header;
        if (!storage_->read({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
            return false;
        }

        if (header.valid()) {
            set_block_taken(map_, block);
        }
        else {
            set_block_free(map_, block);
        }

        #ifdef PHYLUM_ARDUINO_DEBUG
        sdebug() << "Block " << (uint32_t)block << " is " << (header.valid() ? "TAKEN" : "FREE") << endl;
        #endif
    }

    // These are always taken, anchor blocks and we skip block 0, for now.
    set_block_taken(map_, 0);
    set_block_taken(map_, 1);
    set_block_taken(map_, 2);

    return true;
}

void SerialFlashAllocator::free(block_index_t block) {
    if (!storage_->erase(block)) {
        sdebug() << "Erase failed! (" << (uint32_t)block << ")" << endl;
    }

    set_block_free(map_, block);
}

}

#endif // ARDUINO
