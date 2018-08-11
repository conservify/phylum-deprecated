#ifndef __PHYLUM_FILE_INDEX_H_INCLUDED
#define __PHYLUM_FILE_INDEX_H_INCLUDED

#include <cinttypes>

#include "phylum/private.h"
#include "phylum/backend.h"
#include "phylum/file_allocation.h"

namespace phylum {

struct IndexBlockHead {
    BlockHead block;
    uint64_t position{ 0 };
    uint32_t reserved[4];

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
    uint32_t reserved[4];

    bool valid() {
        return address.valid() && !address.zero();
    }
};

struct IndexBlockTail {
    BlockTail block;
    uint32_t reserved[4];
};

class FileIndex {
    static constexpr size_t NumberOfRegions = 2;

private:
    StorageBackend *storage_{ nullptr };
    FileAllocation *file_{ nullptr };
    BlockAddress head_;

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
    return os << "FileIndex<head=" << e.head_ << ">";
}

}

#endif
