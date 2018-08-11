#ifndef __PHYLUM_BLOCKED_FILE_H_INCLUDED
#define __PHYLUM_BLOCKED_FILE_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/file_index.h"

namespace phylum {

class BlockedFile {
public:
    static constexpr block_index_t IndexFrequency = 8;

private:
    StorageBackend *storage_{ nullptr };
    uint32_t id_{ 0 };
    uint8_t buffer_[SectorSize];
    uint16_t buffavailable_{ 0 };
    uint16_t buffpos_{ 0 };
    uint16_t seek_offset_{ 0 };
    uint32_t bytes_in_block_{ 0 };
    uint32_t position_{ 0 };
    uint32_t length_{ 0 };
    uint32_t version_{ 0 };
    uint32_t blocks_in_file_{ 0 };
    OpenMode mode_{ OpenMode::Read };
    BlockAddress head_;
    BlockAddress beg_;

public:
    BlockedFile() {
    }

    BlockedFile(StorageBackend *storage, OpenMode mode) :
        storage_(storage), mode_(mode) {
    }

    BlockedFile(StorageBackend *storage, OpenMode mode, BlockAddress head) :
        storage_(storage), mode_(mode), head_(head), beg_(head) {
    }

    ~BlockedFile() {
        if (!read_only()) {
            close();
        }
    }

    friend class SimpleFile;

public:
    bool read_only() const;

    uint32_t blocks_in_file() const;

    uint64_t size() const;

    uint64_t tell() const;

    BlockAddress beginning() const;

    BlockAddress head() const;

    uint32_t version() const;

    bool seek(uint64_t position);

    bool seek(BlockAddress from, uint32_t position_at_from, uint64_t bytes);

    int32_t read(uint8_t *ptr, size_t size);

    int32_t write(uint8_t *ptr, size_t size, bool span_sectors = true, bool span_blocks = true);

    int32_t flush();

    bool erase();

    bool initialize();

    bool format();

    void close();

    bool exists();

public:
    BlockAddress end_of_file();

    const Geometry &geometry() const {
        return storage_->geometry();
    }

protected:
    bool tail_sector() const {
        return head_.tail_sector(geometry());
    }

    struct SavedSector {
        int32_t saved;
        BlockAddress head;

        operator bool() {
            return saved > 0;
        }
    };

    struct SeekInfo {
        BlockAddress address;
        uint32_t version;
        int32_t bytes;
        int32_t bytes_in_block;
        int32_t blocks;
    };

    SavedSector save_sector(bool flushing);

    SeekInfo seek_detailed(BlockAddress from, uint32_t position_at_from, uint64_t desired, bool verify_head_block = true);

    BlockAddress initialize(block_index_t block, block_index_t previous);

public:
    virtual block_index_t allocate() = 0;

};

class AllocatedBlockedFile : public BlockedFile {
private:
    BlockAllocator *allocator_;

public:
    AllocatedBlockedFile() {
    }

    AllocatedBlockedFile(StorageBackend *storage, OpenMode mode, BlockAllocator *allocator, BlockAddress head) :
        BlockedFile(storage, mode, head), allocator_(allocator) {
    }

public:
    block_index_t allocate() override;

};

}

#endif
