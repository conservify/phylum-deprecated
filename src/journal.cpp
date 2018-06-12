#include "phylum/journal.h"

namespace phylum {

struct JournalBlockHeader {
    BlockAllocSector header;

    JournalBlockHeader() : header(BlockType::Journal) {
    }

    void fill() {
        header.magic.fill();
        header.age = 0;
        header.timestamp = 0;
    }

    bool valid() const {
        return header.valid();
    }
};

struct JournalTailSector {
    block_index_t linked_block{ BLOCK_INDEX_INVALID };
};

Journal::Journal(StorageBackend &storage, BlockAllocator &allocator) : storage_(&storage), allocator_(&allocator) {
}

bool Journal::format(block_index_t block) {
    if (!initialize_block(block, BLOCK_INDEX_INVALID)) {
        return false;
    }

    location_ = { block, SectorSize };

    return true;
}

bool Journal::locate(block_index_t block) {
    auto &g = storage_->geometry();
    auto location = BlockAddress{ block, 0 };
    while (true) {
        if (location.beginning_of_block()) {
            auto tl = BlockAddress::tail_data_of( location.block, g, sizeof(JournalTailSector));
            JournalTailSector tail;

            if (!storage_->read(tl, &tail, sizeof(JournalTailSector))) {
                return false;
            }

            if (is_valid_block(tail.linked_block)) {
                location = { tail.linked_block, 0 };
            }
            else {
                location.add(SectorSize);
            }
        }
        else {
            JournalEntry entry;

            assert(location.find_room(g, sizeof(JournalEntry)));

            if (!storage_->read(location, &entry, sizeof(JournalEntry))) {
                return false;
            }

            if (!entry.valid()) {
                location_ = location;
                return true;
            }

            location.add(sizeof(JournalEntry));
        }
    }
    return false;
}

bool Journal::append(JournalEntry entry) {
    auto &g = storage_->geometry();
    auto required = sizeof(JournalEntry);

    if (!location_.can_write_entry_before_tail(g, required, sizeof(JournalTailSector))) {
        auto new_block = allocator_->allocate(BlockType::Journal);
        if (!initialize_block(new_block, location_.block)) {
            return false;
        }

        auto tl = BlockAddress::tail_data_of( location_.block, g, sizeof(JournalTailSector));
        JournalTailSector tail;

        tail.linked_block = new_block;
        if (!storage_->write(tl, &tail, sizeof(JournalTailSector))) {
            return false;
        }

        location_ = { new_block, SectorSize };
    }

    assert(location_.find_room(g, required));

    if (!storage_->write(location_, &entry, required)) {
        return false;
    }

    location_.add(sizeof(JournalEntry));

    return true;
}

bool Journal::initialize_block(block_index_t block, block_index_t linked) {
    JournalBlockHeader header;

    header.fill();
    header.header.linked_block = linked;

    if (!storage_->erase(block)) {
        return false;
    }

    if (!storage_->write({ block, 0 }, &header, sizeof(JournalBlockHeader))) {
        return false;
    }

    return true;
}

}
