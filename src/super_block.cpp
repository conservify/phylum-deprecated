#include "phylum/phylum.h"
#include "phylum/private.h"
#include "phylum/super_block.h"

namespace phylum {

constexpr uint16_t SuperBlockStartSector = 0;
constexpr block_index_t WanderingBlockManager::AnchorBlocks[];

WanderingBlockManager::WanderingBlockManager(StorageBackend &storage, ReusableBlockAllocator &blocks) :
    storage_(&storage), blocks_(&blocks) {
}

bool WanderingBlockManager::walk(block_index_t desired, SuperBlockLink &link, SectorAddress &where) {
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

bool WanderingBlockManager::locate() {
    SuperBlockLink link;
    SectorAddress where;

    location_.invalid();

    if (!walk(BLOCK_INDEX_INVALID, link, where)) {
        return false;
    }

    location_ = where;

    return read_super(location_);
}

bool WanderingBlockManager::find_link(block_index_t block, SuperBlockLink &found, SectorAddress &where) {
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

bool WanderingBlockManager::create() {
    block_index_t super_block_block = BLOCK_INDEX_INVALID;
    SuperBlockLink link;
    link.chained_block = BLOCK_INDEX_INVALID;
    link.header.magic.fill();
    link.header.timestamp = chain_length() + 2 + 1;
    link.header.age = 0;

    for (auto i = 0; i < chain_length() + 1; ++i) {
        auto block = blocks_->allocate(i == 0 ? BlockType::SuperBlock : BlockType::SuperBlockLink);
        assert(block != BLOCK_INDEX_INVALID);

        if (!storage_->erase(block)) {
            return false;
        }

        // First of these blocks is actually where the super block goes.
        if (i == 0) {
            super_block_block  = block;
            link_super(link);
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

    if (!write_fresh_super({ super_block_block, SuperBlockStartSector })) {
        return false;
    }

    return locate();
}

bool WanderingBlockManager::rollover(SectorAddress addr, SectorAddress &relocated, PendingWrite pending) {
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

    auto block = blocks_->allocate(pending.type);
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

    blocks_->free(addr.block);

    return true;
}

bool WanderingBlockManager::save() {
    auto write = prepare_super();

    SectorAddress actually_wrote;
    if (!rollover(location_, actually_wrote, write)) {
        return false;
    }

    location_ = actually_wrote;

    return true;
}

int32_t WanderingBlockManager::chain_length() {
    return 2;
}

bool WanderingBlockManager::read(SectorAddress addr, SuperBlockLink &link) {
    return storage_->read({ addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool WanderingBlockManager::write(SectorAddress addr, SuperBlockLink &link) {
    return storage_->write({ addr, 0 }, &link, sizeof(SuperBlockLink));
}

bool WanderingBlockManager::write(SectorAddress addr, PendingWrite write) {
    return storage_->write({ addr, 0 }, write.ptr, write.n);
}

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
