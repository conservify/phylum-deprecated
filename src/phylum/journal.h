#ifndef __PHYLUM_JOURNAL_H_INCLUDED
#define __PHYLUM_JOURNAL_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/block_alloc.h"
#include "phylum/layout.h"

namespace phylum {

enum class JournalEntryType : uint8_t {
    Zeros = 0,
    Allocation,
    Ones = 0xff
};

struct JournalBlockHead {
    BlockHead block;

    JournalBlockHead(BlockType type = BlockType::Journal) : block(type) {
    }

    void fill() {
        block.magic.fill();
        block.age = 0;
        block.timestamp = 0;
    }

    bool valid() const {
        return block.valid();
    }
};

struct JournalEntry {
    JournalEntryType type;
    block_index_t block;
    BlockType block_type;

    bool valid() {
        return type != JournalEntryType::Zeros && type != JournalEntryType::Ones;
    }
};

struct JournalBlockTail {
    BlockTail block;
};


class Journal {
private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    BlockAddress location_;

public:
    Journal(StorageBackend &storage, BlockAllocator &allocator);

public:
    BlockAddress location() {
        return location_;
    }

public:
    bool format(block_index_t block);
    bool locate(block_index_t block);
    bool append(JournalEntry entry);

};

}

#endif
