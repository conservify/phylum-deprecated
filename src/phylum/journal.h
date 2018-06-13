#ifndef __PHYLUM_JOURNAL_H_INCLUDED
#define __PHYLUM_JOURNAL_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/block_alloc.h"

namespace phylum {

enum class JournalEntryType : uint8_t {
    Zeros = 0,
    Allocation,
    Ones = 0xff
};

struct JournalEntry {
    JournalEntryType type;
    block_index_t block;
    BlockType block_type;

    bool valid() {
        return type != JournalEntryType::Zeros && type != JournalEntryType::Ones;
    }
};

struct JournalBlockHead {
    BlockHead header;

    JournalBlockHead(BlockType type = BlockType::Journal) : header(type) {
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

struct JournalBlockTail {
    block_index_t linked_block{ BLOCK_INDEX_INVALID };
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
