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

class IndexBlockLayout {
private:
    StorageBackend *storage_;
    Extent extent_;

public:
    IndexBlockLayout(StorageBackend &storage, Extent extent) : storage_(&storage), extent_(extent) {
    }

public:
    bool format() {
        Extent region = extent_;

        // Format the key index blocks so that we know the index hasn't been
        // written to them yet. Future searches will see formatted blocks and
        // know that the index head is before them.
        while (!region.empty()) {
            if (!storage_->erase(region.middle_block())) {
                return false;
            }
            region = region.first_half();
        }

        // Write the first block in the index.
        if (!write_head(region.start)) {
            return false;
        }

        return true;
    }

    bool seek(uint64_t position, block_index_t &end_block) {
        Extent region = extent_;
        block_index_t valid_block{ BLOCK_INDEX_INVALID };

        // TODO: Shortcut for searching for beginning of file?

        // While we've got an area to search. If a block is initialized we check
        // the position in the file that pertains to that block. If the position
        // is before we search the region after that block.
        while (!region.empty()) {
            IndexBlockHead head(BlockType::Error);

            auto block = region.middle_block();
            if (!read_head(block, head)) {
                return false;
            }

            if (head.valid()) {
                valid_block = block;

                #if PHYLUM_DEBUG > 1
                sdebug() << "Seek: desired=" << position << " at=" << head.position << " index-block=" << block << endl;
                #endif

                if (head.position == position) {
                    end_block = valid_block;
                    return true;
                }

                if (head.position > position) {
                    region = region.first_half();
                }
                else {
                    region = region.second_half();
                }
            }
            else {
                // File is too short to have filled the index to here.
                region = region.first_half();
            }
        }

        // Return the block we found that's close to their desired position.
        end_block = valid_block;

        return true;
    }

private:
    bool read_head(block_index_t block, IndexBlockHead &head) {
        if (!storage_->read({ block, 0 }, &head, sizeof(IndexBlockHead))) {
            return false;
        }

        return true;
    }

    bool write_head(block_index_t block) {
        IndexBlockHead head;
        head.position = 0;
        head.fill();

        if (!storage_->erase(block)) {
            return false;
        }

        if (!storage_->write({ block, 0 }, &head, sizeof(IndexBlockHead))) {
            return false;
        }
        return true;
    }

};

FileIndex::FileIndex() {
}

FileIndex::FileIndex(StorageBackend *storage, FileAllocation *file) : storage_(storage), file_(file) {
}

bool FileIndex::format() {
    auto caching = SectorCachingStorage{ *storage_ };

    IndexBlockLayout sorted{ caching, file_->index };
    if (!sorted.format()) {
        return false;
    }

    head_ = file_->index.beginning();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Formatted: " << *this << endl;
    #endif

    return true;
}

bool FileIndex::initialize() {
    auto caching = SectorCachingStorage{ *storage_ };

    #if PHYLUM_DEBUG > 1
    sdebug() << "Initializing: " << *this << endl;
    #endif

    block_index_t end_block;
    IndexBlockLayout sorted{ caching, file_->index };
    if (!sorted.seek(UINT64_MAX, end_block)) {
        return false;
    }

    IndexRecord record;
    auto layout = get_index_layout(caching, { end_block, 0 });
    while (layout.walk<IndexRecord>(record)) {

    }
    head_ = layout.address();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Initialized: " << *this << endl;
    #endif

    return true;
}

bool FileIndex::seek(uint64_t position, IndexRecord &selected) {
    assert(head_.valid());

    auto caching = SectorCachingStorage{ *storage_ };

    #if PHYLUM_DEBUG > 1
    sdebug() << "Seeking: " << *this << " position=" << position << endl;
    #endif

    block_index_t end_block;
    IndexBlockLayout sorted{ caching, file_->index };
    if (!sorted.seek(position, end_block)) {
        return false;
    }

    IndexRecord record;
    auto reading = get_index_layout(caching, { end_block, 0 });
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
    assert(head_.valid());

    auto caching = SectorCachingStorage{ *storage_ };
    auto allocator = ExtentAllocator{ file_->index, head_.block + 1 };
    auto layout = get_index_layout(caching, allocator, head_);
    auto record = IndexRecord{ position, address };

    IndexBlockHead head;
    head.position = position;
    head.fill();

    if (!layout.append(record, head)) {
        return false;
    }

    head_ = layout.address();

    #ifdef PHYLUM_DEBUG
    sdebug() << "Append: " << *this << " position=" << position << " = " << address << endl;
    #endif

    return true;
}

}
