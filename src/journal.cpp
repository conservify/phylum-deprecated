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

Journal::Journal(StorageBackend &storage) : storage_(&storage) {
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

    return true;
}

}
