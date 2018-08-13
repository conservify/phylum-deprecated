#include "phylum/serial_flash_fs.h"

namespace phylum {

AllocatedBlockedFile Files::open(BlockAddress start, OpenMode mode) {
    return AllocatedBlockedFile{ backend_, mode, allocator_, start };
}

bool UnusedBlockReclaimer::walk(BlockAddress address) {
    auto file = files_->open(address, OpenMode::Read);
    file.walk(&tracker_);
    return true;
}

bool UnusedBlockReclaimer::reclaim() {
    wandering_->walk(&tracker_);

    for (auto block = (uint32_t)0; block < files_->backend_->geometry().number_of_blocks; ++block) {
        if (tracker_.is_free(block)) {
            if (files_->allocator_->is_taken(block)) {
                sdebug() << "Erase: " << block << endl;
                if (!files_->backend_->erase(block)) {
                    return false;
                }
            }
        }
    }

    return true;
}

}
