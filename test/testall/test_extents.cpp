#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

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

static uint64_t file_block_overhead(const Geometry &geometry) {
    auto sectors_per_block = geometry.sectors_per_block();
    return SectorSize + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
}

static uint64_t effective_file_block_size(const Geometry &geometry) {
    return geometry.block_size() - file_block_overhead(geometry);
}

enum class WriteStrategy {
    Append,
    Rolling
};

struct FileDescriptor {
    char name[16];
    WriteStrategy stategy;
    uint64_t maximum_size;
};

struct Extent {
    block_index_t start;
    block_index_t nblocks;

    bool contains(block_index_t block) const {
        return block >= start && block < start + nblocks;
    }

    bool contains(const BlockAddress &address) const {
        return contains((block_index_t)address.block);
    }

    uint64_t size(const Geometry &g) const {
        return nblocks * effective_file_block_size(g);
    }
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

    friend class SimpleFile;
    friend class FileIndex;

public:
    friend std::ostream& operator<<(std::ostream& os, const File &f);

    template<size_t SIZE>
    friend class FileLayout;

};

inline std::ostream& operator<<(std::ostream& os, const Extent &e) {
    return os << "Extent<" << e.start << " - " << e.start + e.nblocks << " l=" << e.nblocks << ">";
}

inline std::ostream& operator<<(std::ostream& os, const File &f) {
    return os << "File<id=" << (int16_t)f.id_ << " index=" << f.index_ << " data=" << f.data_ << ">";
}

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

    bool valid() {
        return address.valid();
    }
};

struct IndexBlockTail {
    BlockTail block;
};

inline std::ostream& operator<<(std::ostream& os, const IndexRecord &f) {
    return os << "IndexRecord<" << f.position << " addr=" << f.address << ">";
}

class ExtentAllocator : public BlockAllocator {
private:
    Extent extent_;
    block_index_t block_;

public:
    ExtentAllocator(Extent extent, block_index_t block) : extent_(extent), block_(block) {
    }

public:
    virtual bool initialize(Geometry &geometry) override {
        assert(false);
    }
    virtual AllocatorState state() override {
        assert(false);
    }
    virtual void state(AllocatorState state) override {
        assert(false);
    }
    virtual block_index_t allocate(BlockType type) override {
        return block_++;
    }
    virtual void free(block_index_t block) override {
        assert(false);
    }

};

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage,
                                                                           BlockAllocator &allocator,
                                                                           BlockAddress address) {
    return { storage, allocator, address, BlockType::Index };
}

class FileIndex {
private:
    StorageBackend *storage_;
    File *file_{ nullptr };
    BlockAddress head_;

public:
    FileIndex() {
    }

    FileIndex(StorageBackend *storage, File *file) : storage_(storage), file_(file) {
    }

public:
    bool initialize() {
        if (head_.valid()) {
            return true;
        }

        head_ = { file_->index_.start, 0 };

        auto allocator = ExtentAllocator{ file_->index_, head_.block };
        auto layout = get_index_layout(*storage_, allocator, head_);

        if (layout.find_append_location<IndexRecord>(head_.block)) {
            head_ = layout.address();
        }

        return true;
    }

    IndexRecord seek() {
        auto allocator = ExtentAllocator{ file_->index_, BLOCK_INDEX_INVALID };
        auto layout = get_index_layout(*storage_, allocator, { file_->index_.start, 0 });

        if (!layout.find_tail_entry<IndexRecord>(file_->index_.start)) {
            return IndexRecord{ };
        }

        IndexRecord record;
        if (!storage_->read(layout.address(), &record, sizeof(IndexRecord))) {
            return IndexRecord{ };
        }

        return record;
    }

    bool append(uint32_t position, BlockAddress address) {
        if (!initialize()) {
            return false;
        }

        auto allocator = ExtentAllocator{ file_->index_, head_.block };
        auto layout = get_index_layout(*storage_, allocator, head_);

        if (!layout.append(IndexRecord{ position, address })) {
            return false;
        }

        head_ = layout.address();

        sdebug() << "Index: " << address << " = " << position << " " << head_ << std::endl;

        return true;
    }

    bool format() {
        if (!storage_->erase(file_->index_.start)) {
            return false;
        }
        if (!storage_->erase(file_->data_.start)) {
            return false;
        }
        return true;
    }

};

class SimpleFile {
private:
    StorageBackend *storage_;
    File *file_{ nullptr };
    uint32_t id_{ 0 };
    uint8_t buffer_[SectorSize];
    uint16_t buffavailable_{ 0 };
    uint16_t buffpos_{ 0 };
    uint32_t bytes_in_block_{ 0 };
    uint32_t position_{ 0 };
    uint32_t length_{ 0 };
    uint8_t blocks_since_save_{ 0 };
    bool readonly_{ false };
    BlockAddress head_;
    FileIndex index_;

public:
    SimpleFile() {
    }

    SimpleFile(StorageBackend *storage, File *file) : storage_(storage), file_(file), index_(storage_, file) {
        head_ = { file_->data_.start, 0 };
    }

public:
    uint64_t maximum_size() const {
        return file_->data_.size(storage_->geometry());
    }

    uint64_t size() const {
        return length_;
    }

    operator bool() const {
        return file_ != nullptr;
    }

    bool seek() {
        if (!index_.initialize()) {
            return false;
        }

        auto end = index_.seek();
        if (end.valid()) {
            head_ = end.address;
            length_ = end.position;
            position_ = end.position;
        }

        auto info = seek(head_.block, UINT64_MAX);
        head_ = info.address;
        length_ += info.bytes;
        position_ += info.bytes;

        return true;
    }

    int32_t write(uint8_t *ptr, size_t size) {
        auto to_write = size;
        auto wrote = 0;

        assert(!readonly_);

        if (!head_.valid()) {
            return 0;
        }

        while (to_write > 0) {
            auto overhead = tail_sector() ? sizeof(FileBlockTail) : sizeof(FileSectorTail);
            auto remaining = sizeof(buffer_) - overhead - buffpos_;
            auto copying = to_write > remaining ? remaining : to_write;

            if (remaining == 0) {
                if (flush() == 0) {
                    return wrote;
                }

                // If we're at the end then don't try and write more.
                if (!head_.valid()) {
                    return wrote;
                }
            }
            else {
                memcpy(buffer_ + buffpos_, (const uint8_t *)ptr + wrote, copying);
                buffpos_ += copying;
                wrote += copying;
                length_ += copying;
                position_ += copying;
                bytes_in_block_ += copying;
                to_write -= copying;
            }
        }

        return wrote;
    }

    int32_t flush() {
        if (readonly_) {
            return 0;
        }

        if (buffpos_ == 0) {
            return 0;
        }

        // It's on us to initialize the first block's header.
        if (head_.block == file_->data_.start && head_.beginning_of_block()) {
            head_ = initialize(head_.block, BLOCK_INDEX_INVALID);
            index_.append(0, { head_.block, 0 });
        }

        // If this is the tail sector in the block write the tail section that links
        // to the following block.
        auto linked = BLOCK_INDEX_INVALID;
        auto writing_tail_sector = tail_sector();
        auto addr = head_;
        if (writing_tail_sector) {
            // Check to see if we're at the end of our allocated space.
            linked = head_.block + 1;
            if (!file_->data_.contains(linked)) {
                linked = BLOCK_INDEX_INVALID;
            }

            FileBlockTail tail;
            tail.sector.bytes = buffpos_;
            tail.bytes_in_block = bytes_in_block_;
            tail.block.linked_block = linked;
            memcpy(tail_info<FileBlockTail>(buffer_), &tail, sizeof(FileBlockTail));
        }
        else {
            FileSectorTail tail;
            tail.bytes = buffpos_;
            memcpy(tail_info<FileSectorTail>(buffer_), &tail, sizeof(FileSectorTail));
            head_.add(SectorSize);
        }

        // Write this full sector. No partial writes here because of the tail. Most
        // of the time we write full sectors anyway.
        assert(file_->data_.contains(addr));

        if (!storage_->write(addr, buffer_, sizeof(buffer_))) {
            return 0;
        }

        // We could do this in the if scope above, I like doing things "in order" though.
        if (writing_tail_sector) {
            if (is_valid_block(linked)) {
                head_ = initialize(linked, head_.block);
                assert(file_->data_.contains(head_));
                if (!head_.valid()) {
                    assert(false); // TODO: Yikes.
                }

                // Every N blocks we save our offset in the tree. This affects how much
                // seeking needs to happen when trying to append or seek around.
                blocks_since_save_++;
                if (blocks_since_save_ == 8) {
                    index_.append(length_, head_);
                    blocks_since_save_ = 0;
                }
            }
            else {
                head_ = BlockAddress{ };
            }

            bytes_in_block_ = 0;
        }

        auto flushed = buffpos_;
        buffpos_ = 0;
        return flushed;
    }

    void close() {
        flush();
    }

    bool format() {
        return index_.format();
    }

private:
    template<typename T, size_t N>
    static T *tail_info(uint8_t(&buffer)[N]) {
        auto tail_offset = sizeof(buffer) - sizeof(T);
        return reinterpret_cast<T*>(buffer + tail_offset);
    }

    struct SeekInfo {
        BlockAddress address;
        int32_t bytes;
        int32_t blocks;
    };

    bool tail_sector() const {
        return head_.tail_sector(storage_->geometry());
    }

    SeekInfo seek(block_index_t starting_block, uint64_t max, bool verify_head_block = true) {
        auto bytes = 0;
        auto blocks = 0;

        // This is used just to sanity check that the block we were given has
        // actually been begun. For example, the very first block won't have been.
        if (verify_head_block) {
            FileBlockHead head;
            if (!storage_->read({ starting_block, 0 }, &head, sizeof(FileBlockHead))) {
                return { };
            }

            if (!head.valid()) {
                return { { starting_block, 0 }, 0, 0 };
            }
        }

        // Start walking the file from the given starting block until we reach the
        // end of the file or we've passed `max` bytes.
        auto &g = storage_->geometry();
        auto addr = BlockAddress::tail_sector_of(starting_block, g);
        while (true) {
            if (!storage_->read(addr, buffer_, sizeof(buffer_))) {
                return { };
            }

            // Check to see if our desired location is in this block, otherwise we
            // can just skip this one entirely.
            if (addr.tail_sector(g)) {
                FileBlockTail tail;
                memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
                if (is_valid_block(tail.block.linked_block) && max > tail.bytes_in_block) {
                    addr = BlockAddress::tail_sector_of(tail.block.linked_block, g);
                    bytes += tail.bytes_in_block;
                    max -= tail.bytes_in_block;
                    blocks++;
                }
                else {
                    addr = BlockAddress{ addr.block, SectorSize };
                }
            }
            else {
                FileSectorTail tail;
                memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));

                if (tail.bytes == 0 || tail.bytes == SECTOR_INDEX_INVALID) {
                    break;
                }
                if (max > tail.bytes) {
                    bytes += tail.bytes;
                    max -= tail.bytes;
                    addr.add(SectorSize);
                }
                else {
                    bytes += max;
                    addr.add(max);
                    break;
                }
            }
        }

        return { addr, bytes, blocks };
    }

    BlockAddress initialize(block_index_t block, block_index_t previous) {
        FileBlockHead head;

        head.fill();
        head.file_id = id_;
        head.block.linked_block = previous;

        if (!storage_->erase(block)) {
            return { };
        }

        if (!storage_->write({ block, 0 }, &head, sizeof(FileBlockHead))) {
            return { };
        }

        return BlockAddress { block, SectorSize };
    }

};

template<size_t SIZE>
class FileLayout {
private:
    StorageBackend *storage_;
    File files_[SIZE];

public:
    FileLayout(StorageBackend &storage) : storage_(&storage) {
    }

    Geometry &geometry() const {
        return storage_->geometry();
    }

public:
    bool allocate(FileDescriptor*(&fds)[SIZE]) {
        block_index_t head = 0;

        sdebug() << "Effective block size: " << effective_file_block_size(geometry()) <<
            " overhead = " << file_block_overhead(geometry()) << std::endl;

        for (size_t i = 0; i < SIZE; ++i) {
            auto fd = fds[i];
            auto nblocks = block_index_t(0);

            assert(fd != nullptr);

            if (fd->maximum_size > 0) {
                nblocks = blocks_required_for_size(fd->maximum_size);
            }
            else {
                nblocks = geometry().number_of_blocks - head;
            }

            assert(nblocks > 0);

            auto index = Extent{ head, 2 };
            head += 2;

            auto data = Extent{ head, nblocks };
            head += nblocks;

            files_[i] = File{ *fd, (uint8_t)i, index, data };

            sdebug() << "Allocated: " << files_[i] << " " << fd->name << std::endl;
        }
        return true;
    }

    bool format() {
        for (auto &file : files_) {
            auto f = open(*file.fd_);
            if (!f.format()) {
                return false;
            }
        }

        return true;
    }

public:
    block_index_t blocks_required_for_size(uint64_t opaque_size) {
        constexpr uint64_t Megabyte = (1024 * 1024);
        constexpr uint64_t Kilobyte = 1024;
        uint64_t scale = 0;

        if (geometry().size() < 1024 * Megabyte) {
            scale = Kilobyte;
        }
        else {
            scale = Megabyte;
        }

        auto size = opaque_size * scale;
        return (size / effective_file_block_size(geometry())) + 1;
    }

    SimpleFile open(FileDescriptor &fd) {
        for (size_t i = 0; i < SIZE; ++i) {
            if (files_[i].fd_ == &fd) {
                return SimpleFile{ storage_, &files_[i] };
            }
        }
        return SimpleFile{ nullptr, nullptr };
    }

};

TEST_F(ExtentsSuite, SmallFileWritingToEnd) {
    FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd =   { "startup.log",   WriteStrategy::Append,  100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_log_startup_fd);
    ASSERT_TRUE(file);

    uint64_t total = 0;
    uint8_t data[128] = { 0xcc };
    for (auto i = 0; i < (1024 * 1024) / (int32_t)sizeof(data); ++i) {
        total += file.write(data, sizeof(data));
    }

    ASSERT_EQ(total, file.maximum_size());
}

TEST_F(ExtentsSuite, StandardLayoutAllocating) {
    FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd =   { "startup.log",   WriteStrategy::Append,  100 };
    FileDescriptor file_log_now_fd =       { "now.log",       WriteStrategy::Rolling, 100 };
    FileDescriptor file_log_emergency_fd = { "emergency.log", WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk",       WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
        &file_log_now_fd,
        &file_log_emergency_fd,
        &file_data_fk
    };

    FileLayout<5> layout{ storage_ };

    layout.allocate(files);
    layout.format();
}

TEST_F(ExtentsSuite, LargeFileWritingToEnd) {
    FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk",       WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_data_fk);
    ASSERT_TRUE(file);

    auto total = 0;
    uint8_t data[128] = { 0xcc };
    for (auto i = 0; i < (1024 * 1024) / (int32_t)sizeof(data); ++i) {
        total += file.write(data, sizeof(data));
    }

    ASSERT_EQ(total, 1024 * 1024);
}

TEST_F(ExtentsSuite, LargeFileAppending) {
    FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk",       WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    constexpr uint64_t OneMegabyte = 1024 * 1024;

    {
        auto file = layout.open(file_data_fk);
        ASSERT_TRUE(file);
        ASSERT_EQ(file.size(), (uint64_t)0);

        uint64_t total = 0;
        uint8_t data[128] = { 0xcc };
        for (auto i = 0; i < (int32_t)OneMegabyte / (int32_t)sizeof(data); ++i) {
            total += file.write(data, sizeof(data));
        }

        file.close();

        ASSERT_EQ(file.size(), OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    {
        auto file = layout.open(file_data_fk);
        ASSERT_TRUE(file);
        ASSERT_TRUE(file.seek());
        ASSERT_EQ(file.size(), OneMegabyte);

        uint64_t total = 0;
        uint8_t data[128] = { 0xcc };
        for (auto i = 0; i < (int32_t)OneMegabyte / (int32_t)sizeof(data); ++i) {
            total += file.write(data, sizeof(data));
        }

        file.close();

        ASSERT_EQ(file.size(), 2 * OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }
}
