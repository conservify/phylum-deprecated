#ifndef __PHYLUM_FILES_H_INCLUDED
#define __PHYLUM_FILES_H_INCLUDED

#include "phylum/private.h"
#include "phylum/blocked_file.h"
#include "phylum/wandering_block_manager.h"
#include "backends/arduino_serial_flash/serial_flash_allocator.h"

namespace phylum {

class Files {
private:
    StorageBackend *backend_;
    SerialFlashAllocator *allocator_;

public:
    Files(StorageBackend *backend, SerialFlashAllocator *allocator) : backend_(backend), allocator_(allocator) {
    }

    friend class UnusedBlockReclaimer;

public:
    AllocatedBlockedFile open(BlockAddress start, OpenMode mode);

};

}

#endif
