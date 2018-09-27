#ifndef __PHYLUM_TREE_FS_SUPER_BLOCK_H_INCLUDED
#define __PHYLUM_TREE_FS_SUPER_BLOCK_H_INCLUDED

#include "phylum/super_block_manager.h"

namespace phylum {

struct TreeFileSystemSuperBlock : public MinimumSuperBlock  {
    AllocatorState allocator;
    timestamp_t last_gc{ 0 };
    block_index_t tree{ 0 };
    block_index_t journal{ BLOCK_INDEX_INVALID };
    block_index_t free{ BLOCK_INDEX_INVALID };
    BlockAddress leaf;
    BlockAddress index;

    TreeFileSystemSuperBlock() {
    }
};

class TreeFileSystemSuperBlockManager {
private:
    BlockManager *blocks_;
    SuperBlockManager manager_;
    TreeFileSystemSuperBlock sb_;

public:
    TreeFileSystemSuperBlock &block() {
        return sb_;
    }

    timestamp_t timestamp() {
        return sb_.link.header.timestamp;
    }

    SectorAddress location() {
        return manager_.location();
    }

public:
    TreeFileSystemSuperBlockManager(StorageBackend &storage, BlockManager &blocks);

public:
    bool locate();
    bool create();
    bool save();

protected:
    void update_before_create();

};

}

#endif
