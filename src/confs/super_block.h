#ifndef __CONFS_SUPER_BLOCK_H_INCLUDED
#define __CONFS_SUPER_BLOCK_H_INCLUDED

#include "confs/backend.h"
#include "confs/private.h"
#include "confs/block_alloc.h"

namespace confs {

struct SuperBlockLink {
    BlockHeader header;
    sector_index_t sector;
    block_index_t chained_block;
};

struct SuperBlock {
    SuperBlockLink link;
    uint32_t number_of_files;
    block_index_t tree;
};

class SuperBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    confs_sector_addr_t location_;
    SuperBlock sb_;

public:
    SuperBlockManager(StorageBackend &storage, BlockAllocator &allocator);

public:
    confs_sector_addr_t location() {
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
    bool walk(block_index_t desired, SuperBlockLink &link, confs_sector_addr_t &where);
    bool find_link(block_index_t block, SuperBlockLink &found, confs_sector_addr_t &where);

    struct PendingWrite {
        void *ptr;
        size_t n;
    };
    bool rollover(confs_sector_addr_t addr, confs_sector_addr_t &new_location, PendingWrite write);

    bool read(confs_sector_addr_t addr, SuperBlockLink &link);
    bool write(confs_sector_addr_t addr, SuperBlockLink &link);
    bool read(confs_sector_addr_t addr, SuperBlock &sb);
    bool write(confs_sector_addr_t addr, SuperBlock &sb);
    bool write(confs_sector_addr_t addr, PendingWrite write);

};

}

#endif
