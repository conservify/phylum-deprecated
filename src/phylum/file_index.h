#ifndef __PHYLUM_FILE_INDEX_H_INCLUDED
#define __PHYLUM_FILE_INDEX_H_INCLUDED

#include <cinttypes>

#include "phylum/private.h"
#include "phylum/backend.h"
#include "phylum/file_allocation.h"

namespace phylum {

struct IndexBlockHead {
    BlockHead block;

    IndexBlockHead() : block(BlockType::Index) {
    }

    IndexBlockHead(BlockType type) : block(type) {
    }

    void fill() {
        block.fill();
    }

    bool valid() {
        return block.valid();
    }
};

struct IndexRecord {
    uint64_t position;
    BlockAddress address;

    bool valid() {
        return address.valid() && !address.zero();
    }
};

struct IndexBlockTail {
    BlockTail block;
};

class FileIndex {
    static constexpr size_t NumberOfRegions = 2;

private:
    StorageBackend *storage_;
    FileAllocation *file_{ nullptr };
    BlockAddress beginning_;
    BlockAddress head_;
    BlockAddress end_;

public:
    FileIndex();
    FileIndex(StorageBackend *storage, FileAllocation *file);

    friend ostreamtype& operator<<(ostreamtype& os, const FileIndex &e);

public:
    bool initialize();

    bool format();

    bool seek(uint64_t position, IndexRecord &recod);

    bool append(uint32_t position, BlockAddress address);

};

inline ostreamtype& operator<<(ostreamtype& os, const IndexRecord &f) {
    return os << "IndexRecord<" << f.position << " addr=" << f.address << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const FileIndex &e) {
    return os << "FileIndex<" <<
        " beginning=" << e.beginning_ <<
        " end=" << e.end_ <<
        " head=" << e.head_ <<
        ">";
}

}

#endif
