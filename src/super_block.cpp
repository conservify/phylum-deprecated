#include "phylum/phylum.h"
#include "phylum/private.h"
#include "phylum/super_block.h"

namespace phylum {

SuperBlockManager::SuperBlockManager(StorageBackend &storage, BlockManager &blocks) : WanderingBlockManager(storage, blocks) {
}

void SuperBlockManager::link_super(SuperBlockLink link) {
    sb_.link = link;
    sb_.link.header.type = BlockType::SuperBlock;
}

bool SuperBlockManager::read_super(SectorAddress addr) {
    if (!read(addr, sb_)) {
        return false;
    }

    reinterpret_cast<BlockManager*>(blocks_)->state(sb_.allocator);

    return true;
}

bool SuperBlockManager::write_fresh_super(SectorAddress addr) {
    // We pull allocator state after doing the above allocations to ensure the
    // first state we write is correct.
    sb_.tree = BLOCK_INDEX_INVALID;
    sb_.journal = blocks_->allocate(BlockType::Journal);
    sb_.free = blocks_->allocate(BlockType::Free);
    sb_.allocator = reinterpret_cast<BlockManager*>(blocks_)->state();

    assert(sb_.journal != BLOCK_INDEX_INVALID);
    assert(sb_.free != BLOCK_INDEX_INVALID);

    if (!write(addr, sb_)) {
        return false;
    }

    return true;
}

WanderingBlockManager::PendingWrite SuperBlockManager::prepare_super() {
    sb_.link.header.timestamp++;
    sb_.allocator = reinterpret_cast<BlockManager*>(blocks_)->state();

    return PendingWrite{
        BlockType::SuperBlock,
        &sb_,
        sizeof(SuperBlock)
    };
}

bool SuperBlockManager::read(SectorAddress addr, SuperBlock &sb) {
    return storage_->read({ addr, 0 }, &sb, sizeof(SuperBlock));
}

bool SuperBlockManager::write(SectorAddress addr, SuperBlock &sb) {
    return storage_->write({ addr, 0 }, &sb, sizeof(SuperBlock));
}

}
