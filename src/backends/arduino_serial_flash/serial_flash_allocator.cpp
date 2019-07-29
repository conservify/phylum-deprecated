#include "serial_flash_allocator.h"

namespace phylum {

SerialFlashAllocator::SerialFlashAllocator(StorageBackend &storage) : storage_(&storage) {
    for (auto i = 0; i < PreallocationSize; ++i) {
        preallocated_[i] = BLOCK_INDEX_INVALID;
    }
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

AllocatedBlock SerialFlashAllocator::allocate(BlockType type) {
    for (auto i = 0; i < PreallocationSize; ++i) {
        if (is_valid_block(preallocated_[i])) {
            auto f = preallocated_[i];
            preallocated_[i] = BLOCK_INDEX_INVALID;
            return { f, 0, true };
        }
    }

    return allocate_internal(type);
}

AllocatedBlock SerialFlashAllocator::allocate_internal(BlockType type) {
    ScanInfo info;

    if (!scan(true, info)) {
        sdebug() << "Failed to allocate! (" << type << ")" << endl;
        return { };
    }

    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "Allocate: " << type << " = " << (uint32_t)info.block << " " << info.age << endl;
    #endif

    assert(info.block != BLOCK_INDEX_INVALID);

    set_block_taken(map_, info.block);

    return { info.block, info.age, false };
}

bool SerialFlashAllocator::initialize() {
    ScanInfo info;

    if (!scan(false, info)) {
        return false;
    }

    sdebug() << "Allocator ready: " << number_of_free_blocks() << endl;

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

bool SerialFlashAllocator::scan(bool free_only, ScanInfo *blocks, size_t size) {
    size_t n = 0;

    for (size_t block = 0; block < size; block++) {
        blocks[block] = { BLOCK_INDEX_INVALID, 0 };
    }

    for (auto block = (uint32_t)3; block < storage_->geometry().number_of_blocks; ++block) {
        if (free_only) {
            if (!is_block_free(map_, block)) {
                continue;
            }
        }

        BlockHead candidate;
        if (is_taken(block, candidate)) {
            set_block_taken(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is taken. (age=" << candidate.age << ", " << candidate.type << ")" << endl;
            #endif
        }
        else {
            set_block_free(map_, block);
            #ifdef PHYLUM_ARDUINO_DEBUG
            sdebug() << "Block " << (uint32_t)block << " is free. (age=" << candidate.age << ")" << (candidate.valid() ? "" : " Invalid") << endl;
            #endif

            if (size == 1) {
                if (candidate.valid()) {
                    if (blocks->block == BLOCK_INDEX_INVALID || candidate.age < blocks->age) {
                        blocks->block = block;
                        blocks->age = candidate.age;
                    }
                }
                else {
                    // Using 0 here means no other "valid" block can win, so we're
                    // picking this invalid block regardless.
                    blocks->block = block;
                    blocks->age = 0;
                }
            }
            else {
                if (candidate.valid()) {
                    blocks[n].block = block;
                    blocks[n].age = candidate.age;
                    n++;
                }
                else {
                    // Using 0 here means no other "valid" block can win, so we're
                    // picking this invalid block regardless.
                    blocks[n].block = block;
                    blocks[n].age = 0;
                    n++;
                }

                if (n == size) {
                    break;
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

bool SerialFlashAllocator::scan(bool free_only, ScanInfo &selected) {
    selected.block = BLOCK_INDEX_INVALID;
    selected.age = BLOCK_AGE_INVALID;

    if (!scan(free_only, &selected, 1)) {
        return false;
    }

    return true;
}

bool SerialFlashAllocator::free(block_index_t block, block_age_t age) {
    BlockHead header;
    if (!storage_->read(BlockAddress{ block, 0 }, &header, sizeof(BlockHead))) {
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

static SerialFlashAllocator::ScanInfo *take_block(SerialFlashAllocator::ScanInfo *available, size_t size) {
    SerialFlashAllocator::ScanInfo *selected = nullptr;

    for (size_t i = 0; i < size; ++i) {
        auto &candidate = available[i];

        if (candidate.block != BLOCK_INDEX_INVALID) {
            if (selected == nullptr || candidate.age < selected->age || candidate.age == 0) {
                selected = &candidate;
            }
        }
    }

    return selected;
}

bool SerialFlashAllocator::preallocate(uint32_t expected_size) {
    // This is risk, though given this backend we're sure this will never be
    // more than 64. So this will nearly always be less than 1k.
    auto nblocks = storage_->geometry().number_of_blocks;
    auto available = (ScanInfo *)alloca(sizeof(ScanInfo) * nblocks);

    if (!scan(true, available, nblocks)) {
        return false;
    }

    for (auto i = 0; i < PreallocationSize; ++i) {
        auto alloc = take_block(available, nblocks);

        assert(alloc != nullptr);

        set_block_taken(map_, alloc->block);

        if (!storage_->erase(alloc->block)) {
            return false;
        }

        preallocated_[i] = alloc->block;

        alloc->block = BLOCK_INDEX_INVALID;
        alloc->age = BLOCK_AGE_INVALID;
    }
    return true;
}

TakenBlockTracker::TakenBlockTracker() {
    for (auto block = (uint32_t)0; block < sizeof(map_) * 8; ++block) {
        set_block_free(map_, block);
    }

    set_block_taken(map_, 0);
    set_block_taken(map_, 1);
    set_block_taken(map_, 2);
}

void TakenBlockTracker::block(VisitInfo info) {
    auto block = info.block;
    if (block < sizeof(map_) * 8) {
        set_block_taken(map_, block);
    }
}

bool TakenBlockTracker::is_free(block_index_t block) {
    if (block < sizeof(map_) * 8) {
        return is_block_free(map_, block);
    }
    return false;
}

}
