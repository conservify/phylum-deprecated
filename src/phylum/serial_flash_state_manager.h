#ifndef __PHYLUM_SERIAL_FLASH_STATE_MANAGER_H_INCLUDED
#define __PHYLUM_SERIAL_FLASH_STATE_MANAGER_H_INCLUDED

#include "phylum/wandering_block_manager.h"

namespace phylum {

template<typename T>
class SerialFlashStateManager : public WanderingBlockManager {
private:
    T state_;

public:
    T &state() {
        return state_;
    }

public:
    SerialFlashStateManager(StorageBackend &storage, ReusableBlockAllocator &blocks) : WanderingBlockManager(storage, blocks) {
    }

protected:
    bool read(SectorAddress addr, T &sb) {
        return storage_->read({ addr, 0 }, &sb, sizeof(T));
    }

    bool write(SectorAddress addr, T &sb) {
        return storage_->write({ addr, 0 }, &sb, sizeof(T));
    }

    virtual void link_super(SuperBlockLink link) override {
        state_.link = link;
        state_.link.header.type = BlockType::SuperBlock;
    }

    virtual bool read_super(SectorAddress addr) override {
        if (!read(addr, state_)) {
            return false;
        }
        return true;
    }

    virtual bool write_fresh_super(SectorAddress addr) override {
        if (!write(addr, state_)) {
            return false;
        }

        return true;
    }

    virtual PendingWrite prepare_super() override {
        state_.link.header.timestamp++;

        return PendingWrite{
            BlockType::SuperBlock,
            &state_,
            sizeof(T)
        };
    }

};

}

#endif
