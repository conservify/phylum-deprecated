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

bool Journal::locate(block_index_t block) {
    auto &g = storage_->geometry();
    auto location = BlockAddress{ block, SectorSize };
    while (true) {
        JournalTailSector tail;
        auto tail_location = BlockAddress{ location.block, g.block_size() - (uint32_t)sizeof(JournalTailSector) };

        if (!storage_->read(tail_location, &tail, sizeof(JournalTailSector))) {
            return false;
        }

        if (tail.linked_block != BLOCK_INDEX_INVALID && tail.linked_block > 0) {
            location = { tail.linked_block, SectorSize };
        }
        else {
            while (true) {
                JournalEntry entry;
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
    }
    return false;
}

bool Journal::format(block_index_t block) {
    JournalBlockHeader header;

    header.fill();
    header.header.linked_block = BLOCK_INDEX_INVALID;

    if (!storage_->erase(block)) {
        return false;
    }

    if (!storage_->write({ block, 0 }, &header, sizeof(JournalBlockHeader))) {
        return false;
    }

    location_ = { block, SectorSize };

    return true;
}

bool Journal::append(JournalEntry entry) {
    auto &g = storage_->geometry();
    auto required = sizeof(JournalEntry);

    if (!location_.can_write_entry_before_tail(g, required, sizeof(JournalTailSector))) {
        auto tail_location = BlockAddress{ location_.block, g.block_size() - (uint32_t)sizeof(JournalTailSector) };
        auto block = allocator_->allocate(BlockType::Journal);

        if (!format(block)) {
            return false;
        }

        JournalTailSector tail;
        tail.linked_block = block;
        if (!storage_->write(tail_location, &tail, sizeof(JournalTailSector))) {
            return false;
        }

        location_ = { block, SectorSize };
    }

    if (!storage_->write(location_, &entry, required)) {
        return false;
    }

    location_.add(sizeof(JournalEntry));

    return true;
}

}
