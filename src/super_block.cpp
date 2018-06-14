#include "phylum/phylum.h"
#include "phylum/private.h"
#include "phylum/super_block.h"

namespace phylum {

constexpr uint16_t SuperBlockStartSector = 0;
constexpr block_index_t SuperBlockManager::AnchorBlocks[];

SuperBlockManager::SuperBlockManager(StorageBackend &storage, BlockAllocator &allocator) :
    storage_(&storage), allocator_(&allocator) {
}

bool SuperBlockManager::walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where) {
    link = { };
    where.invalid();

    // Find link in anchor block so we can follow the chain from there.
    for (auto block : AnchorBlocks) {
        if (!find_link(block, link, where)) {
            return false;
        }
    }

    // No link in the anchor area!
    if (!where.valid()) {
        return false;
    }

    // See if we're being asked for the child of an anchor block.
    if (desired != BLOCK_INDEX_INVALID && link.chained_block == desired) {
        return true;
    }

    for (auto i = 0; i < chain_length() + 1; ++i) {
        if (!find_link(link.chained_block, link, where)) {
            return false;
        }

        if (where.valid()) {
            if (link.chained_block == desired) {
                return true;
            }
        }
        else {
            break;
        }
    }

    return false;
}

bool SuperBlockManager::locate() {
    SuperBlockLink link;
    SectorAddress where;

    location_.invalid();

    if (!walk(BLOCK_INDEX_INVALID, link, where)) {
        return false;
    }

    location_ = where;

    if (!read(location_, sb_)) {
        return false;
    }

    allocator_->state(sb_.allocator);

    return true;
}

bool SuperBlockManager::find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where) {
    for (auto s = SuperBlockStartSector; s < storage_->geometry().sectors_per_block(); ++s) {
        SuperBlockLink link;

        if (!read({ block, s }, link)) {
            return false;
        }

        if (link.header.magic.valid()) {
            if (found.header.timestamp == TIMESTAMP_INVALID || link.header.timestamp > found.header.timestamp) {
                found = link;
                where = { block, s };
            }
        }
        else {
            break;
        }
    }

    return true;
}

bool SuperBlockManager::create() {
    block_index_t super_block_block = BLOCK_INDEX_INVALID;
    SuperBlockLink link;
    link.chained_block = BLOCK_INDEX_INVALID;
    link.header.magic.fill();
    link.header.timestamp = chain_length() + 2 + 1;
    link.header.age = 0;

    for (auto i = 0; i < chain_length() + 1; ++i) {
        auto block = allocator_->allocate(i == 0 ? BlockType::SuperBlock : BlockType::SuperBlockLink);
        assert(block != BLOCK_INDEX_INVALID);

        if (!storage_->erase(block)) {
            return false;
        }

        // First of these blocks is actually where the super block goes.
        if (i == 0) {
            super_block_block  = block;
            sb_.link = link;
            sb_.link.header.type = BlockType::SuperBlock;
        }
        else {
            if (!write({ block, SuperBlockStartSector }, link)) {
                return false;
            }
        }

        link.chained_block = block;
        link.header.timestamp--;
    }

    // Overwrite both so an older one doesn't confuse us.
    for (auto anchor : AnchorBlocks) {
        link.header.type = BlockType::Anchor;

        if (!storage_->erase(anchor)) {
            return false;
        }

        if (!write({ anchor, SuperBlockStartSector }, link)) {
            return false;
        }

        link.header.timestamp--;
    }

    // We pull allocator state after doing the above allocations to ensure the
    // first state we write is correct.
    sb_.tree = BLOCK_INDEX_INVALID;
    sb_.journal = allocator_->allocate(BlockType::Journal);
    sb_.free = allocator_->allocate(BlockType::Free);
    sb_.allocator = allocator_->state();

    assert(sb_.journal != BLOCK_INDEX_INVALID);
    assert(sb_.free != BLOCK_INDEX_INVALID);

    if (!write({ super_block_block, SuperBlockStartSector }, sb_)) {
        return false;
    }

    return locate();
}

bool SuperBlockManager::rollover(SectorAddress addr, SectorAddress &relocated, PendingWrite pending) {
    // Move to the following sector and see if we need to perform the rollover.
    addr.sector++;

    if (addr.sector < storage_->geometry().sectors_per_block()) {
        relocated = addr;
        return write(relocated, pending);
    }

    // We rollover the anchor blocks in a unique way.
    constexpr auto number_of_anchors = (int32_t)(sizeof(AnchorBlocks) / sizeof(AnchorBlocks[0]));
    for (auto i = 0; i < number_of_anchors; ++i) {
        if (AnchorBlocks[i] == addr.block) {
            relocated = {
                AnchorBlocks[(i + 1) % number_of_anchors],
                SuperBlockStartSector
            };

            if (!storage_->erase(relocated.block)) {
                return false;
            }

            return write(relocated, pending);
        }
    }

    auto block = allocator_->allocate(pending.type);
    relocated = { block, SuperBlockStartSector };
    if (!storage_->erase(block)) {
        return false;
    }

    if (!write(relocated, pending)) {
        return false;
    }

    // Find the chain link that references this now obsolete location.
    SuperBlockLink link;
    SectorAddress previous;
    if (!walk(addr.block, link, previous)) {
        return false;
    }

    link.header.timestamp++;
    link.chained_block = block;

    auto link_write = PendingWrite {
        BlockType::SuperBlockLink,
        &link,
        sizeof(SuperBlockLink)
    };

    SectorAddress actually_wrote;
    if (!rollover(previous, actually_wrote, link_write)) {
        return false;
    }

    allocator_->free(addr.block);

    return true;
}

bool SuperBlockManager::save() {
    sb_.link.header.timestamp++;
    sb_.allocator = allocator_->state();

    auto sb_write = PendingWrite{
        BlockType::SuperBlock,
        &sb_,
        sizeof(SuperBlock)
    };

    SectorAddress actually_wrote;
    if (!rollover(location_, actually_wrote, sb_write)) {
        return false;
    }

    location_ = actually_wrote;

    return true;
}

int32_t SuperBlockManager::chain_length() {
    return 2;
}

bool SuperBlockManager::read(SectorAddress addr, SuperBlockLink &link) {
    return storage_->read({ addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::write(SectorAddress addr, SuperBlockLink &link) {
    return storage_->write({ addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::read(SectorAddress addr, SuperBlock &sb) {
    return storage_->read({ addr, 0 }, &sb, sizeof(SuperBlock));
}

bool SuperBlockManager::write(SectorAddress addr, SuperBlock &sb) {
    return storage_->write({ addr, 0 }, &sb, sizeof(SuperBlock));
}

bool SuperBlockManager::write(SectorAddress addr, PendingWrite write) {
    return storage_->write({ addr, 0 }, write.ptr, write.n);
}

}
