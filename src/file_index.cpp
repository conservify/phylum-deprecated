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
    auto layout = get_index_layout(caching, { file_->index.start, 0 });

    if (!storage_->erase(file_->index.start)) {
        return false;
    }

    if (!layout.write_head(file_->index.start)) {
        return false;
    }

    beginning_ = { file_->index.start, 0 };
    head_ = beginning_;

    #ifdef PHYLUM_DEBUG
    sdebug() << "Formatted: " << *this << endl;
    #endif

    return true;
}

constexpr uint16_t INVALID_VERSION = ((uint16_t)-1);

bool FileIndex::initialize() {
    auto caching = SectorCachingStorage{ *storage_ };
    auto layout = get_index_layout(caching, { file_->index.start, 0 });
    auto skipped = false;
    auto expected_entries = 0;
    auto entries = 0;

    version_ = INVALID_VERSION;
    beginning_ = layout.address();

    #if PHYLUM_DEBUG > 1
    sdebug() << "Initializing: " << *this << endl;
    #endif

    IndexRecord record;
    while (layout.walk<IndexRecord>(record)) {
        if (version_ == INVALID_VERSION || record.version > version_) {
            if (version_ != record.version) {
                if (record.position != 0) {
                    if (!skipped) {
                        // Skip to following block. This happens if we wrapped
                        // around while reindexing, so this is one way we can
                        // find the beginning again. Not sure if this is better
                        // than just ensuring the new index is on its own block.
                        // This is only temporary though, until the reindex
                        // happens again.
                        layout.address({ layout.address().block + 1, 0 });
                        skipped = true;
                    }
                    continue;
                }
                beginning_ = layout.address();
                entries_ = 1;
                entries = 1;
            }
            version_ = record.version;
        }
        else if (version_ == record.version) {
            end_ = layout.address();
            entries_++;
            entries++;
        }

        if (entries > expected_entries) {
            expected_entries = entries;
        }
    }

    if (entries != expected_entries) {
        assert(entries < expected_entries);
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

        if (record.version == version_) {
            if (position == record.position) {
                selected = record;
                break;
            }
            else if (record.position > position) {
                break;
            }

            selected = record;
        }
    }

    #ifdef PHYLUM_DEBUG
    sdebug() << "Seek: " << *this << " position=" << position << " = " << selected << endl;
    #endif

    return true;
}

FileIndex::ReindexInfo FileIndex::append(uint32_t position, BlockAddress address, bool rollover) {
    assert(beginning_.valid());
    assert(head_.valid());

    if (rollover || version_ >= 1) {
        return reindex(position, address);
    }

    entries_++;

    auto caching = SectorCachingStorage{ *storage_ };
    auto allocator = ExtentAllocator{ file_->index, head_.block + 1 };
    auto layout = get_index_layout(caching, allocator, head_);
    auto record = IndexRecord{ position, address, version_, entries_ };
    if (!layout.append(record)) {
        return { };
    }

    head_ = layout.address();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Append: " << record << " head=" << head_ << " beg=" << beginning_ << endl;
    #endif

    return { position, 0 };
}

FileIndex::ReindexInfo FileIndex::reindex(uint64_t length, BlockAddress new_end) {
    assert(beginning_.valid());
    assert(head_.valid());

    auto allocator = ExtentAllocator{ file_->index, head_.block + 1 };
    auto writing = get_index_layout(*storage_, allocator, head_);
    auto caching = SectorCachingStorage{ *storage_ };
    auto reading = get_index_layout(caching, beginning_);

    version_++;
    beginning_ = head_;

    #ifdef PHYLUM_DEBUG
    sdebug() << "Reindex: version=" << version_ << " " << reading.address() << " -> " << writing.address() <<
        " length-before = " << length << " new-end = " << new_end << " entries = " << entries_ << endl;
    #endif

    uint64_t offset = 0;
    IndexRecord record;
    while (reading.walk<IndexRecord>(record)) {
        #ifdef PHYLUM_DEBUG
        sdebug() << "  Old: " << reading.address() << " " << record << endl;
        #endif
        if (record.version == version_ - 1) {
            if (record.position == 0) {
                // If offset is non-zero then we've looped around.
                if (offset != 0) {
                    break;
                }
            }
            else {
                if (offset == 0) {
                    offset = record.position;
                }

                auto nrecord = IndexRecord{ record.position - offset, record.address, version_ };
                #if PHYLUM_DEBUG > 1
                sdebug() << "  " << nrecord << " " << writing.address() << endl;
                #endif
                if (!writing.append(nrecord)) {
                    return { };
                }
            }
        }
    }

    auto new_length = length - offset;

    auto nrecord = IndexRecord{ new_length, new_end, version_ };
    #if PHYLUM_DEBUG > 1
    sdebug() << "  " << nrecord << " " << writing.address() << endl;
    #endif
    if (!writing.append(nrecord)) {
        return { };
    }

    #if PHYLUM_DEBUG > 1
    sdebug() << "  Done: new-length = " << new_length << endl;
    #endif

    head_ = writing.address();

    return { new_length, offset };
}

void FileIndex::dump() {
    assert(beginning_.valid());
    assert(head_.valid());

    auto caching = SectorCachingStorage{ *storage_ };
    auto layout = get_index_layout(caching, { file_->index.start, 0 });

    sdebug() << "Index: " << layout.address() << endl;

    IndexRecord record;
    while (layout.walk<IndexRecord>(record)) {
        sdebug() << "  " << record << " " << layout.address() << endl;
    }
}

}
