#include "phylum/free_pile.h"

namespace phylum {

inline BlockLayout<FreePileBlockHead, FreePileBlockTail> get_layout(StorageBackend &storage,
                                                                    BlockAllocator &allocator,
                                                                    BlockAddress address) {
    return { storage, allocator, address, BlockType::Free };
}

FreePileManager::FreePileManager(StorageBackend &storage, BlockAllocator &allocator)
    : storage_(&storage), allocator_(&allocator) {
}

bool FreePileManager::format(block_index_t block) {
    auto layout = get_layout(*storage_, *allocator_, BlockAddress{ block, 0 });

    if (!layout.write_head(block)) {
        return false;
    }

    location_ = { block, SectorSize };

    return true;
}

bool FreePileManager::locate(block_index_t block) {
    auto layout = get_layout(*storage_, *allocator_, BlockAddress{ block, 0 });

    if (!layout.find_end<FreePileEntry>(block)) {
        return false;
    }

    location_ = layout.address();

    return true;
}

bool FreePileManager::append(FreePileEntry entry) {
    auto layout = get_layout(*storage_, *allocator_, location_);

    if (!layout.append(entry)) {
        return false;
    }

    location_ = layout.address();

    return true;
}

}
