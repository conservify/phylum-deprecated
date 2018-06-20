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
#include <phylum/preallocated.h>

std::map<uint64_t, uint64_t> random_data();

namespace phylum {

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
        for (size_t i = 0; i < sizeof(data_); ++i) {
            data_[i] = i;
        }
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

}

#endif
