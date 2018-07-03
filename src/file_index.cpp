#include "phylum/file_index.h"
#include "phylum/layout.h"

namespace phylum {

class ExtentAllocator : public BlockAllocator {
private:
    Extent extent_;
    block_index_t block_;

public:
    ExtentAllocator(Extent extent, block_index_t block) : extent_(extent), block_(block) {
    }

public:
    virtual block_index_t allocate(BlockType type) override {
        auto b = block_++;
        if (!extent_.contains(b)) {
            b = extent_.start;
        }
        assert(extent_.contains(b));
        return b;
    }

};

static inline BlockLayout<IndexBlockHead, IndexBlockTail>
get_index_layout(StorageBackend &storage, BlockAllocator &allocator, BlockAddress address) {
    return { storage, allocator, address, BlockType::Index };
}

static inline BlockLayout<IndexBlockHead, IndexBlockTail>
get_index_layout(StorageBackend &storage, BlockAddress address) {
        return get_index_layout(storage, empty_allocator, address);
}

FileIndex::FileIndex() {
}

FileIndex::FileIndex(StorageBackend *storage, FileAllocation *file) : storage_(storage), file_(file) {
}

bool FileIndex::format() {
    auto caching = SectorCachingStorage{ *storage_ };
    auto layout = get_index_layout(caching, file_->index.beginning());
    if (!layout.write_head(file_->index.start)) {
        return false;
    }

    beginning_ = file_->index.beginning();
    head_ = beginning_;

    #ifdef PHYLUM_DEBUG
    sdebug() << "Formatted: " << *this << endl;
    #endif

    return true;
}

bool FileIndex::initialize() {
    auto caching = SectorCachingStorage{ *storage_ };
    auto layout = get_index_layout(caching, file_->index.beginning());

    beginning_ = layout.address();

    #if PHYLUM_DEBUG > 1
    sdebug() << "Initializing: " << *this << endl;
    #endif

    IndexRecord record;
    while (layout.walk<IndexRecord>(record)) {
        end_ = layout.address();
    }
    head_ = layout.address();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Initialized: " << *this << endl;
    #endif

    return true;
}

bool FileIndex::seek(uint64_t position, IndexRecord &selected) {
    assert(head_.valid());
    assert(beginning_.valid());

    auto caching = SectorCachingStorage{ *storage_ };
    auto reading = get_index_layout(caching, beginning_);

    #if PHYLUM_DEBUG > 1
    sdebug() << "Seeking: " << *this << " position=" << position << endl;
    #endif

    IndexRecord record;
    while (reading.walk<IndexRecord>(record)) {
        #if PHYLUM_DEBUG > 1
        sdebug() << "  " << record << " " << reading.address() << endl;
        #endif

        if (position == record.position) {
            selected = record;
            break;
        }
        else if (record.position > position) {
            break;
        }

        selected = record;
    }

    #ifdef PHYLUM_DEBUG
    sdebug() << "Seek: " << *this << " position=" << position << " = " << selected << endl;
    #endif

    return true;
}

bool FileIndex::append(uint32_t position, BlockAddress address) {
    assert(beginning_.valid());
    assert(head_.valid());

    auto caching = SectorCachingStorage{ *storage_ };
    auto allocator = ExtentAllocator{ file_->index, head_.block + 1 };
    auto layout = get_index_layout(caching, allocator, head_);
    auto record = IndexRecord{ position, address };
    if (!layout.append(record)) {
        return false;
    }

    head_ = layout.address();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Append: " << record << " head=" << head_ << " beginning=" << beginning_ << endl;
    #endif

    return true;
}

}
