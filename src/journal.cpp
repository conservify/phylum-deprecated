#include "phylum/journal.h"
#include "phylum/layout.h"

namespace phylum {

static inline BlockLayout<JournalBlockHead, JournalBlockTail> get_layout(StorageBackend &storage,
                                                                         BlockAllocator &allocator,
                                                                         BlockAddress address) {
    return { storage, allocator, address, BlockType::Journal };
}

Journal::Journal(StorageBackend &storage, BlockAllocator &allocator)
    : storage_(&storage), allocator_(&allocator) {
}

bool Journal::format(block_index_t block) {
    auto layout = get_layout(*storage_, *allocator_, BlockAddress{ block, 0 });

    if (!layout.write_head(block)) {
        return false;
    }

    location_ = { block, SectorSize };

    return true;
}

bool Journal::locate(block_index_t block) {
    auto layout = get_layout(*storage_, *allocator_, BlockAddress{ block, 0 });

    if (!layout.find_append_location<JournalEntry>(block)) {
        return false;
    }

    location_ = layout.address();

    return true;
}

bool Journal::append(JournalEntry entry) {
    auto layout = get_layout(*storage_, *allocator_, location_);

    if (!layout.append(entry)) {
        return false;
    }

    location_ = layout.address();

    return true;
}

}
