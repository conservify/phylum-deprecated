#ifndef __PHYLUM_SERIAL_FLASH_STATE_MANAGER_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_STATE_MANAGER_H_INCLUDED

#include "phylum/super_block_manager.h"

namespace phylum {

template<typename T>
class BasicSuperBlockManager {
private:
    SuperBlockManager manager_;
    T state_;

public:
    T &state() {
        return state_;
    }

    SectorAddress location() {
        return manager_.location();
    }

    SuperBlockManager &manager() {
        return manager_;
    }

public:
    BasicSuperBlockManager(StorageBackend &storage, ReusableBlockAllocator &blocks) : manager_(storage, blocks) {
    }

public:
    bool locate() {
        if (!manager_.locate(state_, sizeof(T))) {
            return false;
        }

        return true;
    }

    bool create() {
        state_ = T{ };

        if (!manager_.create(state_, sizeof(T))) {
            return false;
        }

        return true;
    }

    bool save() {
        if (!manager_.save(state_, sizeof(T))) {
            return false;
        }

        return true;
    }

};

}

#endif
