#ifndef __PHYLUM_FILE_INDEX_H_INCLUDED
#define __PHYLUM_FILE_INDEX_H_INCLUDED

#include <cinttypes>

#include "phylum/private.h"
#include "phylum/backend.h"
#include "phylum/file_allocation.h"

namespace phylum {

struct IndexBlockHead {
    BlockHead block;
    uint16_t version{ 0 };

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
    uint16_t version;
    uint32_t entries;
    uint32_t reserved[2];

    bool valid() {
        return address.valid() && !address.zero();
    }
};

struct IndexBlockTail {
    BlockTail block;
};

class FileIndex {
private:
    StorageBackend *storage_;
    FileAllocation *file_{ nullptr };
    uint16_t version_{ 0 };
    BlockAddress beginning_;
    BlockAddress head_;
    BlockAddress end_;
    uint32_t entries_{ 0 };

public:
    FileIndex();
    FileIndex(StorageBackend *storage, FileAllocation *file);

    friend ostreamtype& operator<<(ostreamtype& os, const FileIndex &e);

public:
    struct ReindexInfo {
        uint64_t length;
        uint64_t truncated;

        ReindexInfo() : length(0), truncated(0) {
        }

        ReindexInfo(uint64_t length, uint64_t truncated) : length(length), truncated(truncated) {
        }

        operator bool() {
            return length > 0;
        }
    };

public:
    uint16_t version() {
        return version_;
    }

    bool initialize();

    bool format();

    bool seek(uint64_t position, IndexRecord &recod);

    ReindexInfo append(uint32_t position, BlockAddress address, bool rollover = false);

    void dump();

private:
    ReindexInfo reindex(uint64_t length, BlockAddress new_end);

};

inline ostreamtype& operator<<(ostreamtype& os, const IndexRecord &f) {
    return os << "IndexRecord<" << f.version << ": " << f.position << " addr=" << f.address << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const FileIndex &e) {
    return os << "FileIndex<version=" << e.version_ <<
        " beginning=" << e.beginning_ <<
        " end=" << e.end_ <<
        " head=" << e.head_ << ">";
}

}

#endif
