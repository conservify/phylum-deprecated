#ifndef __PHYLUM_SIMPLE_FILE_H_INCLUDED
#define __PHYLUM_SIMPLE_FILE_H_INCLUDED

#include "phylum/backend.h"
#include "phylum/file_descriptor.h"
#include "phylum/file_allocation.h"
#include "phylum/file_index.h"

namespace phylum {

class SimpleFile {
private:
    StorageBackend *storage_;
    FileDescriptor *fd_{ nullptr };
    FileAllocation *file_{ nullptr };
    uint32_t id_{ 0 };
    uint8_t buffer_[SectorSize];
    uint16_t buffavailable_{ 0 };
    uint16_t buffpos_{ 0 };
    uint16_t seek_offset_{ 0 };
    uint32_t bytes_in_block_{ 0 };
    uint32_t position_{ 0 };
    uint32_t length_{ 0 };
    uint32_t version_{ 0 };
    int8_t blocks_since_save_{ 0 };
    OpenMode mode_{ OpenMode::Read };
    BlockAddress head_;
    FileIndex index_;

public:
    static constexpr block_index_t IndexFrequency = 8;

    SimpleFile() {
    }

    SimpleFile(StorageBackend *storage, FileDescriptor *fd, FileAllocation *file, OpenMode mode) :
        storage_(storage), fd_(fd), file_(file), mode_(mode), index_(storage_, file) {
    }

    ~SimpleFile() {
        if (!read_only()) {
            close();
        }
    }

    template<size_t SIZE>
    friend class FileLayout;

public:
    operator bool() const {
        return file_ != nullptr;
    }

    bool read_only() {
        return mode_ == OpenMode::Read;
    }

    FileDescriptor &fd() const;

    bool in_final_block() const;

    uint64_t maximum_size() const;

    uint64_t size() const;

    uint64_t tell() const;

    uint32_t truncated() const;

    BlockAddress head() const;

    FileAllocation &allocation() const {
        return *file_;
    }

    FileIndex &index();

    uint32_t version() const {
        return version_;
    }

public:
    bool seek(uint64_t position);

    int32_t read(uint8_t *ptr, size_t size);

    int32_t write(uint8_t *ptr, size_t size, bool span_sectors = true, bool span_blocks = true);

    int32_t flush();

    bool erase();

    void close() {
        flush();
    }

private:
    struct SavedSector {
        int32_t saved;
        BlockAddress head;

        operator bool() {
            return saved > 0;
        }
    };

    SavedSector save_sector(bool flushing);

    const Geometry &geometry() const {
        return storage_->geometry();
    }

    bool tail_sector() const {
        return head_.tail_sector(geometry());
    }

private:
    bool initialize();

    bool format();

    bool index(BlockAddress address);

    struct SeekInfo {
        BlockAddress address;
        uint32_t version;
        int32_t bytes;
        int32_t bytes_in_block;
        int32_t blocks;
    };

    SeekInfo seek(block_index_t starting_block, uint64_t max, bool verify_head_block = true);

    BlockAddress initialize(block_index_t block, block_index_t previous);

};

}

#endif
