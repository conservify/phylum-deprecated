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

enum class WriteStrategy {
    Append,
    Rolling
};

struct FileDescriptor {
    char name[16];
    WriteStrategy strategy;
    uint64_t maximum_size;
};

static uint64_t file_block_overhead(const Geometry &geometry) {
    auto sectors_per_block = geometry.sectors_per_block();
    return SectorSize + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
}

static uint64_t effective_file_block_size(const Geometry &geometry) {
    return geometry.block_size() - file_block_overhead(geometry);
}

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

    BlockAddress final_sector(const Geometry &g) const {
        return { start + nblocks - 1, g.block_size() - SectorSize };
    }

    BlockAddress end(const Geometry &g) const {
        return { start + nblocks, 0 };
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
    return os << "IndexRecord<" << f.version << ": " << f.position << " addr=" << f.address << ">";
}

class ExtentAllocator : public Allocator {
private:
    Extent extent_;
    block_index_t block_;

public:
    ExtentAllocator(Extent extent, block_index_t block) : extent_(extent), block_(block) {
    }

public:
    virtual block_index_t allocate(BlockType type) override {
        return block_++;
    }

};

static uint64_t index_block_overhead(const Geometry &geometry) {
    return SectorSize + sizeof(IndexBlockTail);
}

static uint64_t effective_index_block_size(const Geometry &geometry) {
    return geometry.block_size() - index_block_overhead(geometry);
}

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage,
                                                                           Allocator &allocator,
                                                                           BlockAddress address) {
    return { storage, allocator, address, BlockType::Index };
}

class FileIndex {
private:
    StorageBackend *storage_;
    File *file_{ nullptr };
    uint16_t version_{ 0 };
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

        if (!layout.append(IndexRecord{ position, address, version_ })) {
            return false;
        }

        head_ = layout.address();

        sdebug() << "Index: " << address << " = " << position << " head=" << head_ << std::endl;

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

    bool reindex() {
        auto allocator = ExtentAllocator{ file_->index_, head_.block };
        auto layout = get_index_layout(*storage_, allocator, BlockAddress{ file_->index_.start, 0 });

        sdebug() << "Reindex: " << head_ << std::endl;

        version_++;

        uint64_t offset = 0;
        IndexRecord record;
        while (layout.walk<IndexRecord>(record)) {
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

                if (!append(record.position - offset, record.address)) {
                    return false;
                }
            }
        }

        return true;
    }

    void dump() {
        auto allocator = ExtentAllocator{ file_->index_, head_.block };
        auto layout = get_index_layout(*storage_, allocator, BlockAddress{ file_->index_.start, 0 });

        sdebug() << "Index: " << std::endl;

        IndexRecord record;
        while (layout.walk<IndexRecord>(record)) {
            sdebug() << "  " << record << std::endl;
        }
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
    uint16_t seek_offset_{ 0 };
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

    ~SimpleFile() {
        if (!readonly_) {
            // close();
        }
    }

public:
    uint64_t maximum_size() const {
        return file_->data_.size(storage_->geometry());
    }

    uint64_t size() const {
        return length_;
    }

    FileIndex &index() {
        return index_;
    }

    operator bool() const {
        return file_ != nullptr;
    }

    bool seek(uint64_t position = 0) {
        if (!index_.initialize()) {
            return false;
        }

        if (position == UINT64_MAX) {
            auto end = index_.seek();
            if (end.valid()) {
                head_ = end.address;
                length_ = end.position;
                position_ = end.position;
            }
        }
        else {
            head_ = { file_->data_.start, 0 };
        }

        auto info = seek(head_.block, position);

        seek_offset_ = info.address.sector_offset(storage_->geometry());
        head_ = info.address;
        head_.add(-seek_offset_);
        length_ += info.bytes;
        position_ += info.bytes;

        return true;
    }

    int32_t read(uint8_t *ptr, size_t size) {
        // Are we out of data to return?
        if (buffavailable_ == buffpos_) {
            buffpos_ = 0;

            if (file_->data_.end(storage_->geometry()) == head_) {
                return 0;
            }

            if (head_.beginning_of_block()) {
                head_.add(SectorSize);
            }

            // sdebug() << "Read: " << head_ << std::endl;

            if (!storage_->read(head_, buffer_, sizeof(buffer_))) {
                return 0;
            }

            // See how much data we have in this sector and/or if we have a block we
            // should be moving onto after this sector is read.
            if (tail_sector()) {
                FileBlockTail tail;
                memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
                buffavailable_ = tail.sector.bytes;
                if (tail.block.linked_block != BLOCK_INDEX_INVALID) {
                    head_ = BlockAddress{ tail.block.linked_block, SectorSize };
                }
                else {
                    // We should be in the last sector of the file.
                    assert(file_->data_.final_sector(storage_->geometry()) == head_);
                    head_ = file_->data_.end(storage_->geometry());
                    assert(file_->data_.end(storage_->geometry()) == head_);
                }
            }
            else {
                FileSectorTail tail;
                memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));
                buffavailable_ = tail.bytes;
                head_.add(SectorSize);
            }

            // sdebug() << "Read: " << head_ << " " << buffavailable_ << std::endl;

            // End of the file? Marked by an "unwritten" sector.
            if (buffavailable_ == 0 || buffavailable_ == SECTOR_INDEX_INVALID) {
                // If we're at the end we know our length.
                if (length_ == ((uint32_t)-1)) {
                    assert(false);
                    length_ = position_;
                }
                buffavailable_ = 0;
                return 0;
            }

            if (seek_offset_ > 0) {
                buffpos_ = seek_offset_;
                seek_offset_ = 0;
            }
        }

        auto remaining = (uint16_t)(buffavailable_ - buffpos_);
        auto copying = remaining > size ? size : remaining;
        memcpy(ptr, buffer_ + buffpos_, copying);

        buffpos_ += copying;
        position_ += copying;

        return copying;
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

    block_index_t rollover() {
        sdebug() << "Rollover" << std::endl;

        index_.reindex();

        return file_->data_.start;
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
                switch (file_->fd_->strategy) {
                case WriteStrategy::Append: {
                    linked = BLOCK_INDEX_INVALID;
                    break;
                }
                case WriteStrategy::Rolling: {
                    linked = rollover();
                    break;
                }
                }
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

        // sdebug() << "Write: " << addr << " " << buffpos_ << std::endl;

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
            auto index_blocks = block_index_t(0);

            assert(fd != nullptr);

            if (fd->maximum_size > 0) {
                nblocks = blocks_required_for_data(fd->maximum_size);
                index_blocks = blocks_required_for_index(nblocks);
            }
            else {
                nblocks = geometry().number_of_blocks - head - 1;
                index_blocks = blocks_required_for_index(nblocks);
                nblocks -= index_blocks;
            }

            assert(nblocks > 0);

            auto index = Extent{ head, index_blocks };
            head += index.nblocks;
            assert(geometry().contains(BlockAddress{ head, 0 }));

            auto data = Extent{ head, nblocks };
            head += data.nblocks;
            assert(geometry().contains(BlockAddress{ head, 0 }));

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

    SimpleFile open(FileDescriptor &fd) {
        for (size_t i = 0; i < SIZE; ++i) {
            if (files_[i].fd_ == &fd) {
                return SimpleFile{ storage_, &files_[i] };
            }
        }
        return SimpleFile{ nullptr, nullptr };
    }

private:
    block_index_t blocks_required_for_index(block_index_t nblocks) {
        auto indices_per_block = effective_index_block_size(geometry()) / sizeof(IndexRecord);
        auto indices = (nblocks / 8) + 1;
        return std::max((uint64_t)1, indices / indices_per_block) * 2;
    }

    block_index_t blocks_required_for_data(uint64_t opaque_size) {
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


};

class PatternHelper {
private:
    uint8_t data_[128]{ 0xcc };
    uint64_t wrote_{ 0 };
    uint64_t read_{ 0 };

public:
    int32_t size() {
        return sizeof(data_);
    }

    uint64_t bytes_written() {
        return wrote_;
    }

    uint64_t bytes_read() {
        return read_;
    }

    uint64_t write(SimpleFile &file, uint32_t times) {
        uint64_t total = 0;
        for (auto i = 0; i < (int32_t)times; ++i) {
            auto bytes = file.write(data_, sizeof(data_));
            total += bytes;
            wrote_ += bytes;
            if (bytes != sizeof(data_)) {
                break;
            }
        }
        return total;
    }

    uint32_t read(SimpleFile &file) {
        uint8_t buffer[sizeof(data_)];
        uint64_t total = 0;

        assert(sizeof(buffer) % sizeof(data_) == (size_t)0);

        while (true) {
            auto bytes = file.read(buffer, sizeof(buffer));
            if (bytes == 0) {
                break;
            }

            auto i = 0;
            while (i < bytes) {
                auto left = bytes - i;
                auto pattern_position = total % size();
                auto comparing = left > (size() - pattern_position) ? (size() - pattern_position) : left;

                assert(memcmp(buffer + i, data_ + pattern_position, comparing) == 0);

                i += comparing;
                total += comparing;
            }
        }

        return total;
    }

    template<size_t N>
    uint64_t verify_file(FileLayout<N> &layout, FileDescriptor &fd) {
        auto file = layout.open(fd);

        auto bytes_read = read(file);

        return bytes_read;
    }

};

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

TEST_F(ExtentsSuite, SmallFileWritingToEnd) {
    FileDescriptor file_system_area_fd =   { "system",      WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd =   { "startup.log", WriteStrategy::Append,  100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_log_startup_fd);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());

    ASSERT_EQ(total, file.maximum_size());

    file.close();

    auto verified = helper.verify_file(layout, file_log_startup_fd);
    ASSERT_EQ(total, verified);
}

TEST_F(ExtentsSuite, LargeFileWritingToEnd) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_data_fk);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());

    file.close();

    ASSERT_EQ(total, (uint64_t)1024 * 1024);

    auto verified = helper.verify_file(layout, file_data_fk);
    ASSERT_EQ(total, verified);
}

TEST_F(ExtentsSuite, LargeFileAppending) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    constexpr uint64_t OneMegabyte = 1024 * 1024;

    PatternHelper helper;
    {
        auto file = layout.open(file_data_fk);
        ASSERT_TRUE(file);
        ASSERT_EQ(file.size(), (uint64_t)0);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    {
        auto file = layout.open(file_data_fk);
        ASSERT_TRUE(file);
        ASSERT_TRUE(file.seek(UINT64_MAX));
        ASSERT_EQ(file.size(), OneMegabyte);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), 2 * OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    auto verified = helper.verify_file(layout, file_data_fk);
    ASSERT_EQ(OneMegabyte * 2, verified);
}

TEST_F(ExtentsSuite, SeekMiddleOfFile) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    constexpr uint64_t OneMegabyte = 1024 * 1024;
    auto file = layout.open(file_data_fk);
    ASSERT_EQ(file.size(), (uint64_t)0);
    PatternHelper helper;
    auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());
    file.close();

    ASSERT_EQ(file.size(), OneMegabyte);
    ASSERT_EQ(total, OneMegabyte);

    auto middle_on_pattern_edge = (OneMegabyte / 2) / helper.size() * helper.size();
    auto reading = layout.open(file_data_fk);
    ASSERT_EQ(reading.size(), (uint64_t)0);
    reading.seek(middle_on_pattern_edge);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte / 2);
}

TEST_F(ExtentsSuite, RollingWriteStrategyOneRollover) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_data_fk);
    PatternHelper helper;
    auto total = helper.write(file, ((file.maximum_size() + 4096) / helper.size()));

    file.close();

    ASSERT_EQ(total, helper.bytes_written());

    file.index().dump();
}

TEST_F(ExtentsSuite, RollingWriteStrategyTwoRollovers) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    layout.allocate(files);
    layout.format();

    auto file = layout.open(file_data_fk);

    PatternHelper helper;
    auto total = helper.write(file, ((file.maximum_size() * 2 + 4096) / helper.size()));

    file.close();

    ASSERT_EQ(total, helper.bytes_written());

    file.index().dump();
}
