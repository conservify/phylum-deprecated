#include "phylum/phylum.h"
#include "phylum/private.h"
#include "phylum/tree_fs_super_block.h"

namespace phylum {

TreeFileSystemSuperBlockManager::TreeFileSystemSuperBlockManager(StorageBackend &storage, BlockManager &blocks) : blocks_(&blocks), manager_(storage, blocks) {
}

bool TreeFileSystemSuperBlockManager::locate() {
    if (!manager_.locate(sb_, sizeof(TreeFileSystemSuperBlock))) {
        return false;
    }

    blocks_->state(sb_.allocator);

    return true;
}

bool TreeFileSystemSuperBlockManager::create() {
    // We pull allocator state after doing the above allocations to ensure the
    // first state we write is correct.
    sb_ = TreeFileSystemSuperBlock{ };
    sb_.tree = BLOCK_INDEX_INVALID;
    sb_.journal = blocks_->allocate(BlockType::Journal);
    sb_.free = blocks_->allocate(BlockType::Free);

    assert(sb_.journal != BLOCK_INDEX_INVALID);
    assert(sb_.free != BLOCK_INDEX_INVALID);

    if (!manager_.create(sb_, sizeof(TreeFileSystemSuperBlock), [&] { update_before_create(); })) {
        return false;
    }

    return locate();
}

bool TreeFileSystemSuperBlockManager::save() {
    sb_.link.header.timestamp++;
    sb_.allocator = blocks_->state();

    if (!manager_.save(sb_, sizeof(TreeFileSystemSuperBlock))) {
        return false;
    }

    return true;
}

void TreeFileSystemSuperBlockManager::update_before_create() {
    // Creating the super block layout requires allocating, which changes
    // allocator state. This updates just prior to saving.
    sb_.allocator = blocks_->state();
}

}
