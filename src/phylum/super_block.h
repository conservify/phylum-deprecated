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

struct MinimumSuperBlock {
    SuperBlockLink link{ BlockType::SuperBlock };
};

struct SuperBlock : public MinimumSuperBlock  {
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

    virtual void link_super(SuperBlockLink link) override;
    virtual bool read_super(SectorAddress addr) override;
    virtual bool write_fresh_super(SectorAddress addr) override;
    virtual PendingWrite prepare_super() override;

};

template<typename T>
class SerialFlashStateManager : public WanderingBlockManager {
private:
    T state_;

public:
    T &state() {
        return state_;
    }

public:
    SerialFlashStateManager(StorageBackend &storage, ReusableBlockAllocator &blocks) : WanderingBlockManager(storage, blocks) {
    }

protected:
    bool read(SectorAddress addr, T &sb) {
        return storage_->read({ addr, 0 }, &sb, sizeof(T));
    }

    bool write(SectorAddress addr, T &sb) {
        return storage_->write({ addr, 0 }, &sb, sizeof(T));
    }

    virtual void link_super(SuperBlockLink link) override {
        state_.link = link;
        state_.link.header.type = BlockType::SuperBlock;
    }

    virtual bool read_super(SectorAddress addr) override {
        if (!read(addr, state_)) {
            return false;
        }
        return true;
    }

    virtual bool write_fresh_super(SectorAddress addr) override {
        if (!write(addr, state_)) {
            return false;
        }

        return true;
    }

    virtual PendingWrite prepare_super() override {
        state_.link.header.timestamp++;

        return PendingWrite{
            BlockType::SuperBlock,
            &state_,
            sizeof(T)
        };
    }

};

}

#endif
