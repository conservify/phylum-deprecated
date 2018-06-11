#ifndef __PHYLUM_SUPER_BLOCK_H_INCLUDED
#define __PHYLUM_SUPER_BLOCK_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/private.h"
#include "phylum/block_alloc.h"

namespace phylum {

struct SuperBlockLink {
    BlockAllocSector header;
    sector_index_t sector{ 0 };
    block_index_t chained_block{ 0 };

    SuperBlockLink(BlockType type = BlockType::SuperBlockLink) : header(type) {
    }
};

struct SuperBlock {
    SuperBlockLink link;
    block_index_t tree{ 0 };
    block_index_t alloc_head{ BLOCK_INDEX_INVALID };

    SuperBlock() : link(BlockType::SuperBlock) {
    }
};

class SuperBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    // TODO: Store more than one super block in a sector?
    SectorAddress location_;
    SuperBlock sb_;

public:
    SuperBlockManager(StorageBackend &storage, BlockAllocator &allocator);

public:
    SectorAddress location() {
        return location_;
    }

    block_index_t tree_block() {
        return sb_.tree;
    }

    SuperBlock &block() {
        return sb_;
    }

    timestamp_t timestamp() {
        return sb_.link.header.timestamp;
    }

public:
    bool locate();
    bool create();
    bool save();

private:
    int32_t chain_length();
    bool walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where);
    bool find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where);

    struct PendingWrite {
        BlockType type;
        void *ptr;
        size_t n;
    };

    bool rollover(SectorAddress addr, SectorAddress &new_location, PendingWrite write);
    bool read(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, SuperBlockLink &link);
    bool read(SectorAddress addr, SuperBlock &sb);
    bool write(SectorAddress addr, SuperBlock &sb);
    bool write(SectorAddress addr, PendingWrite write);

};

}

#endif
