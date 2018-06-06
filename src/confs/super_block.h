#ifndef __CONFS_SUPER_BLOCK_H_INCLUDED
#define __CONFS_SUPER_BLOCK_H_INCLUDED

#include "confs/backend.h"
#include "confs/private.h"
#include "confs/block_alloc.h"

namespace confs {

struct SuperBlockLink {
    BlockAllocSector header;
    sector_index_t sector{ 0 };
    block_index_t chained_block{ 0 };

    SuperBlockLink(BlockType type = BlockType::SuperBlockLink) : header(type) {
    }
};

struct SuperBlock {
    SuperBlockLink link;
    uint32_t number_of_files{ 0 };
    block_index_t tree{ 0 };

    SuperBlock() : link(BlockType::SuperBlock) {
    }
};

class SuperBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    StorageBackend *storage_;
    BlockAllocator *allocator_;
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

    timestamp_t timestamp() {
        return sb_.link.header.timestamp;
    }

public:
    bool locate();
    bool create();
    bool save(block_index_t new_tree_block);

private:
    int32_t chain_length();
    bool walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where);
    bool find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where);

    struct PendingWrite {
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
