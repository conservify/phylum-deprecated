#ifndef __PHYLUM_SUPER_BLOCK_H_INCLUDED
#define __PHYLUM_SUPER_BLOCK_H_INCLUDED

#include "phylum/serial_flash_state_manager.h"

namespace phylum {

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

}

#endif
