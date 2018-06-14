#ifndef __PHYLUM_FREE_PILE_H_INCLUDED
#define __PHYLUM_FREE_PILE_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/block_alloc.h"
#include "phylum/layout.h"

namespace phylum {

struct FreePileBlockHead {
    BlockHead block;

    FreePileBlockHead(BlockType type = BlockType::Free) : block(type) {
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

struct FreePileEntry {
    block_index_t available;
    block_index_t taken;

    bool valid() {
        return is_valid_block(available) || is_valid_block(taken);
    }
};

struct FreePileBlockTail {
    BlockTail block;
};

class FreePileManager {
private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    BlockAddress location_;

public:
    FreePileManager(StorageBackend &storage, BlockAllocator &allocator);

public:
    BlockAddress location() {
        return location_;
    }

public:
    bool format(block_index_t block);
    bool locate(block_index_t block);
    bool append(FreePileEntry entry);
    bool free(block_index_t block);

};

}

#endif
