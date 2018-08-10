#ifndef __PHYLUM_WANDERING_BLOCK_H_INCLUDED
#define __PHYLUM_WANDERING_BLOCK_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/private.h"
#include "phylum/block_alloc.h"

namespace phylum {

struct SuperBlockLink {
    BlockHead header;
    sector_index_t sector{ 0 };
    block_index_t chained_block{ 0 };

    SuperBlockLink(BlockType type = BlockType::SuperBlockLink) : header(type) {
    }
};

struct MinimumSuperBlock {
    SuperBlockLink link{ BlockType::SuperBlock };
};

class WanderingBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    // TODO: Store more than one super block in a sector?
    SectorAddress location_;

protected:
    StorageBackend *storage_;
    ReusableBlockAllocator *blocks_;

public:
    WanderingBlockManager(StorageBackend &storage, ReusableBlockAllocator &blocks);

public:
    SectorAddress location() {
        return location_;
    }

public:
    bool locate();
    bool create();
    bool save();

protected:
    struct PendingWrite {
        BlockType type;
        void *ptr;
        size_t n;
    };

private:
    int32_t chain_length();
    bool walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where, block_index_t *visited = nullptr);
    bool find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where);

    bool rollover(SectorAddress addr, SectorAddress &new_location, PendingWrite write);
    bool read(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, PendingWrite write);

protected:
    virtual void link_super(SuperBlockLink link) = 0;
    virtual bool read_super(SectorAddress addr) = 0;
    virtual bool write_fresh_super(SectorAddress addr) = 0;
    virtual PendingWrite prepare_super() = 0;

};

}

#endif
