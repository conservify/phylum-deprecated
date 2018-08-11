#include <algorithm>

#include "phylum/phylum.h"
#include "phylum/file_preallocator.h"
#include "size_calcs.h"

namespace phylum {

bool FilePreallocator::allocate(uint8_t id, FileDescriptor *fd, FileAllocation &file) {
    auto nblocks = block_index_t(0);
    auto index_blocks = block_index_t(0);

    assert(fd != nullptr);

    if (fd->maximum_size > 0) {
        nblocks = blocks_required_for_data(fd->maximum_size);
        index_blocks = blocks_required_for_index(nblocks) * 2;
    }
    else {
        nblocks = geometry().number_of_blocks - head_ - 1;
        index_blocks = blocks_required_for_index(nblocks) * 2;
        nblocks -= index_blocks;
    }

    assert(nblocks > 0);

    auto index = Extent{ head_, index_blocks };
    head_ += index.nblocks;
    assert(geometry().contains(BlockAddress{ head_, 0 }));

    auto data = Extent{ head_, nblocks };
    head_ += data.nblocks;
    assert(geometry().contains(BlockAddress{ head_, 0 }));

    file = FileAllocation{ index, data };

    #ifdef PHYLUM_DEBUG
    sdebug() << "Allocated: " << file << " " << fd->name << endl;
    #endif

    return true;
}

block_index_t FilePreallocator::blocks_required_for_index(block_index_t nblocks) {
    auto indices_per_block = effective_index_block_size(geometry()) / sizeof(IndexRecord);
    auto index_entries = (nblocks / BlockedFile::IndexFrequency) + 1;
    return std::max((uint64_t)1, index_entries / indices_per_block);
}

block_index_t FilePreallocator::blocks_required_for_data(uint64_t opaque_size) {
    constexpr uint64_t Megabyte = (1024 * 1024);
    constexpr uint64_t Kilobyte = (1024);
    uint64_t scale = 0;

    if (geometry().size() < 1024 * Megabyte) {
        scale = Kilobyte;
    }
    else {
        scale = Megabyte;
    }

    auto size = opaque_size * scale;
    return (size / effective_file_block_size(geometry())) + 1;
}

}
