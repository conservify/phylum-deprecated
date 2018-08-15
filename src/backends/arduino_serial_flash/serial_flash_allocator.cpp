#include "serial_flash_allocator.h"

namespace phylum {

SerialFlashAllocator::SerialFlashAllocator(StorageBackend &storage) : storage_(&storage) {
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

    if (info.block == BLOCK_INDEX_INVALID) {
        sdebug() << "Allocate Failed" << endl;
        assert(info.block != BLOCK_INDEX_INVALID);
    }

    set_block_taken(map_, info.block);

    return info.block;
}

bool SerialFlashAllocator::initialize() {
    ScanInfo info;

    if (!scan(false, info)) {
        return false;
    }

    return true;
}

bool SerialFlashAllocator::free_all_blocks() {
    for (auto block = (uint32_t)0; block < storage_->geometry().number_of_blocks; ++block) {
        free(block, 0);
    }

    return true;
}

bool SerialFlashAllocator::is_taken(block_index_t block, BlockHead &header) {
    if (!storage_->read({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
        return false;
    }

    auto valid = header.valid();
    auto taken = valid && header.type != BlockType::Unallocated;

    return taken;
}

bool SerialFlashAllocator::is_taken(block_index_t block) {
    BlockHead header;
    return is_taken(block, header);
}

bool SerialFlashAllocator::scan(bool free_only, ScanInfo &info) {
    info.block = BLOCK_INDEX_INVALID;
    info.age = BLOCK_AGE_INVALID;

    for (auto block = (uint32_t)3; block < storage_->geometry().number_of_blocks; ++block) {
        if (free_only) {
            if (!is_block_free(map_, block)) {
                continue;
            }
        }

        BlockHead header;
        if (is_taken(block, header)) {
            set_block_taken(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is taken. (" << header.type << ")" << endl;
            #endif
        }
        else {
            set_block_free(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is free. (age=" << header.age << ")" << endl;
            #endif

            if (header.valid()) {
                if (header.age < info.age || info.age == BLOCK_AGE_INVALID) {
                    info.block = block;
                    info.age = header.age;
                }
            }
            else {
                if (info.block == BLOCK_INDEX_INVALID) {
                    info.block = block;
                    info.age = BLOCK_AGE_INVALID;
                }
            }
        }
    }

    // These are always taken, anchor blocks and we skip block 0, for now.
    set_block_taken(map_, 0);
    set_block_taken(map_, 1);
    set_block_taken(map_, 2);

    return true;
}

bool SerialFlashAllocator::free(block_index_t block, block_age_t age) {
    BlockHead header;
    if (!storage_->read({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
        return false;
    }

    if (header.valid()) {
        age = header.age + 1;
    }

    if (!storage_->erase(block)) {
        sdebug() << "Erase failed! (" << (uint32_t)block << ")" << endl;
        return false;
    }

    header.fill();
    header.age = age;
    header.type = BlockType::Unallocated;
    if (!storage_->write({ (block_index_t)block, 0 }, &header, sizeof(BlockHead))) {
        sdebug() << "Write erased block failed! (" << (uint32_t)block << ")" << endl;
        return false;
    }

    set_block_free(map_, block);

    return true;
}

uint32_t SerialFlashAllocator::number_of_free_blocks() {
    uint32_t c = 0;
    for (auto block = (uint32_t)0; block < storage_->geometry().number_of_blocks; ++block) {
        if (is_block_free(map_, block)) {
            c++;
        }
    }
    return c;
}

TakenBlockTracker::TakenBlockTracker() {
    for (auto block = (uint32_t)3; block < sizeof(map_) * 8; ++block) {
        set_block_free(map_, block);
    }

    set_block_taken(map_, 0);
    set_block_taken(map_, 1);
    set_block_taken(map_, 2);
}

void TakenBlockTracker::block(block_index_t block) {
    if (block < sizeof(map_) * 8) {
        set_block_taken(map_, block);
    }
}

bool TakenBlockTracker::is_free(block_index_t block) {
    if (block > sizeof(map_) * 8) {
        return is_block_free(map_, block);
    }
    return false;
}

}
