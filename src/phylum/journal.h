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

private:
    bool initialize_block(block_index_t block, block_index_t linked = BLOCK_INDEX_INVALID);

};

}

#endif
