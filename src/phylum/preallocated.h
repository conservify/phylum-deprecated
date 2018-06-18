#ifndef __PHYLUM_PREALLOCATED_H_INCLUDED
#define __PHYLUM_PREALLOCATED_H_INCLUDED

#include <algorithm>
#include <cinttypes>

namespace phylum {

enum class WriteStrategy {
    Append,
    Rolling
};

struct FileDescriptor {
    char name[16];
    WriteStrategy strategy;
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
    friend ostreamtype& operator<<(ostreamtype& os, const File &f);

    template<size_t SIZE>
    friend class FileLayout;

};

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

inline ostreamtype& operator<<(ostreamtype& os, const Extent &e) {
    return os << "Extent<" << e.start << " - " << e.start + e.nblocks << " l=" << e.nblocks << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const File &f) {
    return os << "File<id=" << (int16_t)f.id_ << " index=" << f.index_ << " data=" << f.data_ << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const IndexRecord &f) {
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

static uint64_t file_block_overhead(const Geometry &geometry) {
    auto sectors_per_block = geometry.sectors_per_block();
    return SectorSize + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
}

static uint64_t effective_file_block_size(const Geometry &geometry) {
    return geometry.block_size() - file_block_overhead(geometry);
}

static uint64_t index_block_overhead(const Geometry &geometry) {
    return SectorSize + sizeof(IndexBlockTail);
}

static uint64_t effective_index_block_size(const Geometry &geometry) {
    return geometry.block_size() - index_block_overhead(geometry);
}

static EmptyAllocator empty_allocator_;

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage, BlockAddress address) {
    return { storage, empty_allocator_, address, BlockType::Index };
}

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage, Allocator &allocator, BlockAddress address) {
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
    struct ReindexInfo {
        uint64_t length;
        uint64_t truncated;

        ReindexInfo() : length(0), truncated(0) {
        }

        ReindexInfo(uint64_t length, uint64_t truncated) : length(length), truncated(truncated) {
        }

        operator bool() {
            return length > 0;
        }
    };

private:
    bool initialize();

    BlockAddress address() {
        return { file_->index_.start, 0 };
    }

    uint16_t version() {
        return version_;
    }

public:
    bool format();

    IndexRecord seek(uint64_t position);

    ReindexInfo append(uint32_t position, BlockAddress address);

    ReindexInfo reindex(uint64_t length, BlockAddress new_end);

    void dump();

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
    uint32_t truncated_{ 0 };
    int8_t blocks_since_save_{ 0 };
    bool readonly_{ false };
    BlockAddress head_;
    FileIndex index_;

public:
    static constexpr block_index_t IndexFrequency = 4;

    SimpleFile() {
    }

    SimpleFile(StorageBackend *storage, File *file) : storage_(storage), file_(file), index_(storage_, file) {
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

    uint64_t maximum_size() const {
        return file_->data_.nblocks * effective_file_block_size(geometry());
    }

    uint64_t size() const {
        return length_;
    }

    uint64_t tell() const {
        return position_;
    }

    uint32_t truncated() const {
        return truncated_;
    }

    FileIndex &index() {
        return index_;
    }

public:
    bool seek(uint64_t position);

    int32_t read(uint8_t *ptr, size_t size);

    int32_t write(uint8_t *ptr, size_t size);

    int32_t flush();

    void close() {
        flush();
    }

private:
    bool initialize();

    bool format();

    const Geometry &geometry() const {
        return storage_->geometry();
    }

    template<typename T, size_t N>
    static T *tail_info(uint8_t(&buffer)[N]) {
        auto tail_offset = sizeof(buffer) - sizeof(T);
        return reinterpret_cast<T*>(buffer + tail_offset);
    }

    bool tail_sector() const {
        return head_.tail_sector(geometry());
    }

    bool index(BlockAddress address);

    block_index_t rollover();

    struct SeekInfo {
        BlockAddress address;
        int32_t bytes;
        int32_t blocks;
    };

    SeekInfo seek(block_index_t starting_block, uint64_t max, bool verify_head_block = true);

    BlockAddress initialize(block_index_t block, block_index_t previous);

};

template<size_t SIZE>
class FileLayout {
private:
    StorageBackend *storage_;
    File files_[SIZE];

public:
    FileLayout(StorageBackend &storage) : storage_(&storage) {
    }

public:
    bool allocate(FileDescriptor*(&fds)[SIZE]) {
        block_index_t head = 0;

        #ifdef PHYLUM_LAYOUT_DEBUG
        sdebug() << "Effective block size: " << effective_file_block_size(geometry()) <<
            " overhead = " << file_block_overhead(geometry()) << endl;
        #endif

        for (size_t i = 0; i < SIZE; ++i) {
            auto fd = fds[i];
            auto nblocks = block_index_t(0);
            auto index_blocks = block_index_t(0);

            assert(fd != nullptr);

            if (fd->maximum_size > 0) {
                nblocks = blocks_required_for_data(fd->maximum_size);
                index_blocks = blocks_required_for_index(nblocks) * 2;
            }
            else {
                nblocks = geometry().number_of_blocks - head - 1;
                index_blocks = blocks_required_for_index(nblocks) * 2;
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

            #ifdef PHYLUM_LAYOUT_DEBUG
            sdebug() << "Allocated: " << files_[i] << " " << fd->name << endl;
            #endif
        }
        return true;
    }

    SimpleFile open(FileDescriptor &fd) {
        for (size_t i = 0; i < SIZE; ++i) {
            if (files_[i].fd_ == &fd) {
                auto file = SimpleFile{ storage_, &files_[i] };
                file.initialize();
                return file;
            }
        }
        return SimpleFile{ nullptr, nullptr };
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
    Geometry &geometry() const {
        return storage_->geometry();
    }

private:
    block_index_t blocks_required_for_index(block_index_t nblocks) {
        auto indices_per_block = effective_index_block_size(geometry()) / sizeof(IndexRecord);
        auto indices = (nblocks / 8) + 1;
        return std::max((uint64_t)1, indices / indices_per_block);
    }

    block_index_t blocks_required_for_data(uint64_t opaque_size) {
        constexpr uint64_t Megabyte = (1024 * 1024);
        constexpr uint64_t Kilobyte = (1024);
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

}

#endif
