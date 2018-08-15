#ifndef __PHYLUM_FILE_PREALLOCATOR_H_INCLUDED
#define __PHYLUM_FILE_PREALLOCATOR_H_INCLUDED

#include "phylum/backend.h"

namespace phylum {

class FilePreallocator {
private:
    block_index_t head_ = 2;
    Geometry &geometry_;

public:
    FilePreallocator(Geometry &geometry) : geometry_(geometry) {
    }

public:
    bool allocate(uint8_t id, FileDescriptor *fd, FileAllocation &file);

private:
    Geometry &geometry() const {
        return geometry_;
    }

    block_index_t blocks_required_for_index(block_index_t nblocks);

    block_index_t blocks_required_for_data(uint64_t opaque_size);

};

}

#endif
