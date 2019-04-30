#ifndef __PHYLUM_UTILITIES_H_INCLUDED
#define __PHYLUM_UTILITIES_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <iomanip>
#include <string>
#include <algorithm>

#include <phylum/tree.h>
#include <phylum/private.h>
#include <phylum/backend.h>
#include <phylum/block_alloc.h>
#include <phylum/file_system.h>
#include <phylum/simple_file.h>
#include <phylum/file_layout.h>
#include <backends/linux_memory/linux_memory.h>

std::map<uint64_t, uint64_t> random_data();

namespace phylum {

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

class BlockHelper {
private:
    struct BlockInfo {
        std::vector<BlockAddress> live;
    };

    std::map<block_index_t, BlockInfo> blocks;
    StorageBackend *storage_;

public:
    BlockHelper(StorageBackend &storage) : storage_(&storage) {
    }

public:
    bool is_type(block_index_t block, BlockType type);

    void dump(block_index_t first, block_index_t last);

    void dump(block_index_t block);

    void live(std::map<block_index_t, std::vector<BlockAddress>> &live);

    int32_t number_of_chains(BlockType type, block_index_t first = 0, block_index_t last = BLOCK_INDEX_INVALID);

    int32_t number_of_blocks(BlockType type, block_index_t first = 0, block_index_t last = BLOCK_INDEX_INVALID);

    template<typename T>
    std::unique_ptr<T> get_block(block_index_t block) {
        std::unique_ptr<T> p = make_unique<T>();
        assert(storage_->read({ block, 0 }, p.get(), sizeof(T)));
        return p;
    }

};

class DataHelper {
private:
    FileSystem *fs_;

public:
    DataHelper(FileSystem &fs);

public:
    bool write_file(const char *name, size_t size);

};

class PatternHelper {
private:
    uint8_t data_[128];
    uint64_t wrote_{ 0 };
    uint64_t read_{ 0 };

public:
    PatternHelper() {
        fill(data_, sizeof(data_));
    }

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

public:
    void fill(uint8_t *data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            data[i] = i;
        }
    }

    void fill(uint8_t *data, size_t size, uint8_t v) {
        for (size_t i = 0; i < size; ++i) {
            data[i] = v;
        }
    }

    template<typename File>
    uint64_t write(File &file, uint32_t times) {
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

    template<typename File>
    uint32_t read(File &file) {
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
                auto comparing = left > int32_t(size() - pattern_position) ? (size() - pattern_position) : left;

                assert(memcmp(buffer + i, data_ + pattern_position, comparing) == 0);

                i += comparing;
                total += comparing;
            }
        }

        return total;
    }

    template<size_t N>
    uint64_t verify_file(FileLayout<N> &layout, FileDescriptor &fd, int32_t skip = 0) {
        auto file = layout.open(fd);

        if (skip > 0) {
            if (!file.seek(skip)) {
                return 0;
            }
        }

        auto bytes_read = read(file);

        return bytes_read;
    }

};

template <typename T>
class iterate_backwards {
public:
    explicit iterate_backwards(T &t) : t(t) {}
    typename T::reverse_iterator begin() { return t.rbegin(); }
    typename T::reverse_iterator end()   { return t.rend(); }

private:
    T &t;
};

template<typename T>
iterate_backwards<T> backwards(T &t) {
    return iterate_backwards<T>(t);
}

inline static size_t undo_back_to(LinuxMemoryBackend &storage, OperationType type) {
    size_t c = 0;
    for (auto &l : backwards(storage.log().entries())) {
        if (l.can_undo()) {
            l.undo();
            c++;
            if (l.type() == type) {
                break;
            }
        }
    }
    return c;
}

template<typename Predicate>
inline static int32_t undo_everything_after(LinuxMemoryBackend &storage, Predicate predicate, bool log = false) {
    auto c = 0;
    auto seen = false;
    for (auto &l : storage.log().entries()) {
        if (predicate(l)) {
            seen = true;
        }
        if (seen && l.can_undo()) {
            if (log) {
                sdebug() << "Undo: " << l << endl;
            }
            l.undo();
            c++;
        }
    }
    return c;
}

}

#endif
