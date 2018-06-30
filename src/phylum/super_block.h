#ifndef __PHYLUM_SUPER_BLOCK_H_INCLUDED
#define __PHYLUM_SUPER_BLOCK_H_INCLUDED

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

struct SuperBlock {
    SuperBlockLink link{ BlockType::SuperBlock };
    AllocatorState allocator;
    timestamp_t last_gc{ 0 };
    block_index_t tree{ 0 };
    block_index_t journal{ BLOCK_INDEX_INVALID };
    block_index_t free{ BLOCK_INDEX_INVALID };
    BlockAddress leaf{ BLOCK_INDEX_INVALID };
    BlockAddress index{ BLOCK_INDEX_INVALID };

    SuperBlock() {
    }
};

class WanderingBlockManager {
private:
    static constexpr block_index_t AnchorBlocks[] = { 1, 2 };
    // TODO: Store more than one super block in a sector?
    SectorAddress location_;

protected:
    StorageBackend *storage_;
    BlockManager *blocks_;

public:
    WanderingBlockManager(StorageBackend &storage, BlockManager &blocks);

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
    bool walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where);
    bool find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where);

    bool rollover(SectorAddress addr, SectorAddress &new_location, PendingWrite write);
    bool read(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, SuperBlockLink &link);
    bool write(SectorAddress addr, PendingWrite write);

protected:
    virtual void link_tail(SuperBlockLink link) = 0;
    virtual bool read_tail(SectorAddress addr) = 0;
    virtual bool write_fresh(SectorAddress addr) = 0;
    virtual PendingWrite prepare_write() = 0;

};

class SuperBlockManager : public WanderingBlockManager {
private:
    SuperBlock sb_;

public:
    SuperBlock &block() {
        return sb_;
    }

    timestamp_t timestamp() {
        return sb_.link.header.timestamp;
    }

public:
    SuperBlockManager(StorageBackend &storage, BlockManager &blocks);

protected:
    bool read(SectorAddress addr, SuperBlock &sb);
    bool write(SectorAddress addr, SuperBlock &sb);

    virtual void link_tail(SuperBlockLink link) override;
    virtual bool read_tail(SectorAddress addr) override;
    virtual bool write_fresh(SectorAddress addr) override;
    virtual PendingWrite prepare_write() override;

};

}

#endif
