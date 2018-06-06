#include "confs/confs.h"
#include "confs/private.h"
#include "confs/super_block.h"

namespace confs {

constexpr block_index_t SuperBlockManager::AnchorBlocks[];

SuperBlockManager::SuperBlockManager(StorageBackend &storage, BlockAllocator &allocator) :
    storage_(&storage), allocator_(&allocator) {
}

bool SuperBlockManager::walk(block_index_t desired, SuperBlockLink &link, confs_sector_addr_t &where) {
    link = { 0 };
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
    SuperBlockLink link = { 0 };
    confs_sector_addr_t where;

    location_.invalid();

    if (!walk(BLOCK_INDEX_INVALID, link, where)) {
        return false;
    }

    location_ = where;

    if (!read(location_, sb_)) {
        return false;
    }

    return true;
}

bool SuperBlockManager::find_link(block_index_t block, SuperBlockLink &found, confs_sector_addr_t &where) {
    for (auto s = CONFS_SECTOR_HEAD; s < storage_->geometry().sectors_per_block(); ++s) {
        SuperBlockLink link = { 0 };

        if (!read({ block, s }, link)) {
            return false;
        }

        if (link.header.magic.valid()) {
            if (link.header.timestamp > found.header.timestamp) {
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
    SuperBlockLink link;
    link.chained_block = BLOCK_INDEX_INVALID;
    link.header.magic.fill();
    link.header.timestamp = chain_length() + 2 + 1;
    link.header.age = 0;

    for (auto i = 0; i < chain_length() + 1; ++i) {
        auto block = allocator_->allocate();
        assert(block != BLOCK_INDEX_INVALID);

        if (!storage_->erase(block)) {
            return false;
        }

        // First of these blocks is actually where the super block goes.
        if (i == 0) {
            sb_.link = link;
            sb_.tree = allocator_->allocate();

            assert(sb_.tree != BLOCK_INDEX_INVALID);

            if (!write({ block, CONFS_SECTOR_HEAD }, sb_)) {
                return false;
            }
        }
        else {
            if (!write({ block, CONFS_SECTOR_HEAD }, link)) {
                return false;
            }
        }

        link.chained_block = block;
        link.header.timestamp--;
    }

    // Overwrite both so an older one doesn't confuse us.
    for (auto anchor : AnchorBlocks) {
        if (!storage_->erase(anchor)) {
            return false;
        }

        if (!write({ anchor, CONFS_SECTOR_HEAD }, link)) {
            return false;
        }

        link.header.timestamp--;
    }

    return locate();
}

bool SuperBlockManager::rollover(confs_sector_addr_t addr, confs_sector_addr_t &relocated, PendingWrite pending) {
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
                CONFS_SECTOR_HEAD
            };

            if (!storage_->erase(relocated.block)) {
                return false;
            }

            return write(relocated, pending);
        }
    }

    auto block = allocator_->allocate();
    relocated = { block, CONFS_SECTOR_HEAD };
    if (!storage_->erase(block)) {
        return false;
    }

    if (!write(relocated, pending)) {
        return false;
    }

    // Find the chain link that references this now obsolete location.
    SuperBlockLink link = { 0 };
    confs_sector_addr_t previous;
    if (!walk(addr.block, link, previous)) {
        return false;
    }

    link.header.timestamp++;
    link.chained_block = block;

    auto link_write = PendingWrite {
        &link,
        sizeof(SuperBlockLink)
    };

    confs_sector_addr_t actually_wrote;
    if (!rollover(previous, actually_wrote, link_write)) {
        return false;
    }

    allocator_->free(addr.block);

    return true;
}

bool SuperBlockManager::save(block_index_t new_tree_block) {
    sb_.tree = new_tree_block;
    sb_.link.header.timestamp++;

    auto sb_write = PendingWrite{
        &sb_,
        sizeof(SuperBlock)
    };

    confs_sector_addr_t actually_wrote;
    if (!rollover(location_, actually_wrote, sb_write)) {
        return false;
    }

    location_ = actually_wrote;

    return true;
}

int32_t SuperBlockManager::chain_length() {
    return 2;
}

bool SuperBlockManager::read(confs_sector_addr_t addr, SuperBlockLink &link) {
    return storage_->read(addr, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::write(confs_sector_addr_t addr, SuperBlockLink &link) {
    return storage_->write(addr, &link, sizeof(SuperBlockLink));
}

bool SuperBlockManager::read(confs_sector_addr_t addr, SuperBlock &sb) {
    return storage_->read(addr, &sb, sizeof(SuperBlock));
}

bool SuperBlockManager::write(confs_sector_addr_t addr, SuperBlock &sb) {
    return storage_->write(addr, &sb, sizeof(SuperBlock));
}

bool SuperBlockManager::write(confs_sector_addr_t addr, PendingWrite write) {
    return storage_->write(addr, write.ptr, write.n);
}

}
