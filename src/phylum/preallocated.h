#ifndef __PHYLUM_PREALLOCATED_H_INCLUDED
#define __PHYLUM_PREALLOCATED_H_INCLUDED

#include <cinttypes>

#include "phylum/private.h"
#include "phylum/file_index.h"
#include "phylum/file_allocation.h"
#include "phylum/file_table.h"
#include "phylum/file_descriptor.h"

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
    bool readonly_{ false };
    BlockAddress head_;
    FileIndex index_;

public:
    static constexpr block_index_t IndexFrequency = 8;

    SimpleFile() {
    }

    SimpleFile(StorageBackend *storage, FileDescriptor *fd, FileAllocation *file, OpenMode mode) :
        storage_(storage), fd_(fd), file_(file), readonly_(mode == OpenMode::Read), index_(storage_, file) {
    }

    ~SimpleFile() {
        if (!readonly_) {
            close();
        }
    }

    template<size_t SIZE>
    friend class FileLayout;

public:
    operator bool() const {
        return file_ != nullptr;
    }

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

    int32_t write(uint8_t *ptr, size_t size, bool atomic = false);

    int32_t flush();

    bool erase();

    void close() {
        flush();
    }

private:
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

class FilePreallocator {
private:
    block_index_t head_ = 2;
    StorageBackend &storage_;

public:
    FilePreallocator(StorageBackend &storage) : storage_(storage) {
    }

public:
    bool allocate(uint8_t id, FileDescriptor *fd, FileAllocation &file);

private:
    Geometry &geometry() const {
        return storage_.geometry();
    }

    block_index_t blocks_required_for_index(block_index_t nblocks);

    block_index_t blocks_required_for_data(uint64_t opaque_size);

};

struct FileStat {
    uint64_t size;
    uint32_t version;
};

class FileOpener {
public:
    virtual FileStat stat(FileDescriptor &fd) = 0;
    virtual SimpleFile open(FileDescriptor &fd, OpenMode mode) = 0;
    virtual bool erase(FileDescriptor &fd) = 0;

};

template<size_t SIZE>
class FileLayout : public FileOpener {
private:
    StorageBackend *storage_;
    FileDescriptor **fds_;
    FileAllocation allocations_[SIZE];

public:
    FileLayout(StorageBackend &storage) : storage_(&storage) {
    }

public:
    FileAllocation allocation(size_t i) const {
        return allocations_[i];
    }

    bool format(FileDescriptor*(&fds)[SIZE]) {
        FileTable table{ *storage_ };

        fds_ = fds;

        if (!allocate(fds)) {
            return false;
        }

        if (!table.erase()) {
            return false;
        }

        for (size_t i = 0; i < SIZE; ++i) {
            FileTableEntry entry;
            entry.magic.fill();
            memcpy(&entry.fd, fds_[i], sizeof(FileDescriptor));
            memcpy(&entry.alloc, &allocations_[i], sizeof(FileAllocation));
            if (!table.write(entry)) {
                return false;
            }

            auto file = SimpleFile{
                storage_,
                fds_[i],
                &allocations_[i],
                OpenMode::Write
            };
            if (!file.format()) {
                return false;
            }
        }

        return true;
    }

    bool mount(FileDescriptor*(&fds)[SIZE]) {
        FileTable table{ *storage_ };

        fds_ = fds;

        for (size_t i = 0; i < SIZE; ++i) {
            FileTableEntry entry;
            if (!table.read(entry)) {
                return false;
            }

            if (!entry.magic.valid()) {
                return false;
            }

            if (!entry.fd.compatible(fds[i])) {
                return false;
            }

            memcpy(&allocations_[i], &entry.alloc, sizeof(FileAllocation));
        }

        return true;
    }

    bool unmount() {
        fds_ = nullptr;
        for (size_t i = 0; i < SIZE; ++i) {
            allocations_[i] = { };
        }

        return true;
    }

public:
    virtual FileStat stat(FileDescriptor &fd) override {
        auto file = open(fd, OpenMode::Read);
        if (!file) {
            return { 0, 0 };
        }
        auto size = file.size();
        auto version = file.version();
        file.close();
        return { size, version };
    }

    virtual SimpleFile open(FileDescriptor &fd, OpenMode mode = OpenMode::Read) override {
        for (size_t i = 0; i < SIZE; ++i) {
            if (fds_[i] == &fd) {
                auto file = SimpleFile{ storage_, fds_[i], &allocations_[i], mode };
                file.initialize();
                return file;
            }
        }
        return SimpleFile{ nullptr, nullptr, nullptr, OpenMode::Read };
    }

    virtual bool erase(FileDescriptor &fd) override {
        for (size_t i = 0; i < SIZE; ++i) {
            if (fds_[i] == &fd) {
                auto file = SimpleFile{ storage_, fds_[i], &allocations_[i], OpenMode::Write };
                return file.erase();
            }
        }
        return true;
    }

private:
    bool allocate(FileDescriptor*(&fds)[SIZE]) {
        FilePreallocator allocator{ *storage_ };

        #ifdef PHYLUM_LAYOUT_DEBUG
        sdebug() << "Effective block size: " << effective_file_block_size(geometry()) <<
            " overhead = " << file_block_overhead(geometry()) << endl;
        #endif

        for (size_t i = 0; i < SIZE; ++i) {
            if (!allocator.allocate((uint8_t)i, fds[i], allocations_[i])) {
                return false;
            }
        }

        return true;
    }

};

}

#endif
