#ifndef __PHYLUM_BLOCKED_FILE_H_INCLUDED
#define __PHYLUM_BLOCKED_FILE_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/file_index.h"
#include "phylum/file_descriptor.h"
#include "phylum/visitor.h"
#include "phylum/block_alloc.h"
#include "phylum/file.h"

namespace phylum {

class BlockedFile : public File {
public:
    static constexpr block_index_t IndexFrequency = 8;

private:
    StorageBackend *storage_{ nullptr };
    uint32_t id_{ 0 };
    uint8_t buffer_[SectorSize];
    uint16_t buffavailable_{ 0 };
    uint16_t buffpos_{ 0 };
    uint16_t unflushed_{ 0 };
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

    BlockedFile(StorageBackend *storage, uint32_t id, OpenMode mode) :
        storage_(storage), id_(id), mode_(mode) {
    }

    BlockedFile(StorageBackend *storage, uint32_t id, OpenMode mode, BlockAddress head) :
        storage_(storage), id_(id), mode_(mode), head_(head), beg_(head) {
    }

    ~BlockedFile() {
        if (!read_only()) {
            close();
        }
    }

    friend class SimpleFile;

public:
    uint32_t id() const;

    bool read_only() const;

    uint32_t blocks_in_file() const;

    uint64_t size() const override;

    uint64_t tell() const override;

    BlockAddress beginning() const override;

    BlockAddress head() const;

    uint32_t version() const override;

    bool walk(BlockVisitor *visitor);

    bool seek(uint64_t position) override;

    int32_t read(uint8_t *ptr, size_t size) override;

    int32_t write(uint8_t *ptr, size_t size, bool span_sectors = true, bool span_blocks = true) override;

    bool flush();

    bool erase_all_blocks();

    bool erase();

    bool initialize();

    bool format();

    void close() override;

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
        AllocatedBlock allocated;

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

    bool seek(BlockAddress from, uint32_t position_at_from, uint64_t bytes, BlockVisitor *visitor);

    SeekInfo seek(BlockAddress from, uint32_t position_at_from, uint64_t bytes, BlockVisitor *visitor, bool verify_head_block);

    BlockAddress initialize(AllocatedBlock allocated, block_index_t previous);

public:
    virtual AllocatedBlock allocate() = 0;

    virtual void free(block_index_t block) = 0;

};

class AllocatedBlockedFile : public BlockedFile {
private:
    StorageBackend *storage_;
    ReusableBlockAllocator *allocator_;

public:
    AllocatedBlockedFile() {
    }

    AllocatedBlockedFile(StorageBackend *storage, OpenMode mode, ReusableBlockAllocator *allocator, BlockAddress head) :
        BlockedFile(storage, 0, mode, head), storage_(storage), allocator_(allocator) {
    }

public:
    AllocatedBlock allocate() override;

    void free(block_index_t block) override;

    bool preallocate(uint32_t expected_size);

};

}

#endif
