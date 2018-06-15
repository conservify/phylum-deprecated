#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

enum class WriteStrategy {
    Append,
    Rolling
};

struct FileDescriptor {
    char name[16];
    WriteStrategy stategy;
    uint64_t maximum_size;
};

FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
FileDescriptor file_low_startup_fd =   { "startup.log",   WriteStrategy::Append,  100 };
FileDescriptor file_low_now_fd =       { "now.log",       WriteStrategy::Rolling, 100 };
FileDescriptor file_low_emergency_fd = { "emergency.log", WriteStrategy::Append,  100 };
FileDescriptor file_data_fk =          { "data.fk",       WriteStrategy::Append,  0   };

static FileDescriptor* files[] = {
    &file_system_area_fd,
    &file_low_startup_fd,
    &file_low_now_fd,
    &file_low_emergency_fd,
    &file_data_fk
};

class ExtentsSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

    void TearDown() override {
    }

};

struct Extent {
    block_index_t start;
    block_index_t nblocks;
};

struct SeekEntry {
    uint32_t position;
    BlockAddress address;
};

class File {
private:
    FileDescriptor *fd_;
    uint8_t id_;
    Extent index_;
    Extent data_;

public:
    File() {
    }

    File(FileDescriptor &fd, uint8_t id, Extent index, Extent data) : fd_(&fd), id_(id), index_(index), data_(data) {
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const File &f);

    template<size_t SIZE>
    friend class FileLayout;

};

inline std::ostream& operator<<(std::ostream& os, const Extent &e) {
    return os << "Extent<" << e.start << " - " << e.start + e.nblocks << " len=" << e.nblocks << ">";
}

inline std::ostream& operator<<(std::ostream& os, const File &f) {
    return os << "File<id=" << (int16_t)f.id_ << " index=" << f.index_ << " data=" << f.data_ << ">";
}

struct SaveBlock {
    uint64_t bytes_in_block;
};

class SimpleFile {
private:
    File *file_{ nullptr };
    uint8_t buffer_[SectorSize];
    uint16_t bufavailable_{ 0 };
    uint16_t bufpos_{ 0 };
    uint32_t bytes_in_block_{ 0 };
    uint32_t position_{ 0 };
    uint8_t blocks_since_save_{ 0 };
    BlockAddress head_;
    BlockAddress index_;

public:
    SimpleFile() {
    }

    SimpleFile(File *file) : file_(file) {
    }

public:
    int32_t write(uint8_t *data, size_t n) {
        return n;
    }

    operator bool() const {
        return file_ != nullptr;
    }

};

template<size_t SIZE>
class FileLayout {
private:
    Geometry *g_;
    File files_[SIZE];

public:
    FileLayout(Geometry &g) : g_(&g) {
    }

public:
    bool allocate(FileDescriptor*(&fds)[SIZE]) {
        block_index_t head = 0;

        sdebug() << "Effective block size: " << effective_block_size() << " overhead = " << block_overhead() << std::endl;

        for (size_t i = 0; i < SIZE; ++i) {
            auto fd = fds[i];
            auto nblocks = block_index_t(0);

            if (fd->maximum_size > 0) {
                nblocks = blocks_required_for_size(fd->maximum_size);
            }
            else {
                nblocks = g_->number_of_blocks - head;
            }

            assert(nblocks > 0);

            auto data = Extent{ head, nblocks };
            head += nblocks;

            auto index = Extent{ head, 2 };
            head += 2;

            files_[i] = File{ *fd, (uint8_t)i, index, data };

            sdebug() << "Allocated: " << files_[i] << " " << fd->name << std::endl;
        }
        return false;
    }

public:
    block_index_t blocks_required_for_size(uint64_t opaque_size) {
        constexpr uint64_t Megabyte = (1024 * 1024);
        constexpr uint64_t Kilobyte = 1024;
        uint64_t scale = 0;

        if (g_->size() < 1024 * Megabyte) {
            scale = Kilobyte;
        }
        else {
            scale = Megabyte;
        }

        auto size = opaque_size * scale;
        return (size / effective_block_size()) + 1;
    }

    uint64_t block_overhead() {
        auto sectors_per_block = g_->sectors_per_block();
        return sizeof(FileBlockHead) + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
    }

    uint64_t effective_block_size() {
        return g_->block_size() - block_overhead();
    }

    SimpleFile open(FileDescriptor &fd) {
        for (size_t i = 0; i < SIZE; ++i) {
            if (files_[i].fd_ == &fd) {
                return SimpleFile{ &files_[i] };
            }
        }
        return SimpleFile{ nullptr };
    }

};

TEST_F(ExtentsSuite, Example) {
    FileLayout<5> layout{ geometry_ };

    layout.allocate(files);

    auto file = layout.open(file_data_fk);
    ASSERT_TRUE(file);

    uint8_t data[128] = { 0xcc };
    for (auto i = 0; i < (1024 * 1024) / (int32_t)sizeof(data); ++i) {
        file.write(data, sizeof(data));
    }
}
