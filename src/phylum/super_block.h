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

class SuperBlockManager {
private:
    BlockManager *blocks_;
    WanderingBlockManager manager_;
    SuperBlock sb_;

public:
    SuperBlock &block() {
        return sb_;
    }

    timestamp_t timestamp() {
        return sb_.link.header.timestamp;
    }

    SectorAddress location() {
        return manager_.location();
    }

public:
    SuperBlockManager(StorageBackend &storage, BlockManager &blocks);

public:
    bool locate();
    bool create();
    bool save();

protected:
    void update_before_create();

};

}

#endif
