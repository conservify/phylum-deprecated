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

std::map<uint64_t, uint64_t> random_data();

namespace phylum {

class BlockHelper {
private:
    struct BlockInfo {
        std::vector<BlockAddress> live;
    };

    std::map<block_index_t, BlockInfo> blocks;
    StorageBackend *storage_;
    BlockAllocator *allocator_;

public:
    BlockHelper(StorageBackend &storage, BlockAllocator &allocator) : storage_(&storage), allocator_(&allocator) {
    }

public:
    bool is_type(block_index_t block, BlockType type);

    void dump(block_index_t first, block_index_t last);

    void dump(block_index_t block);

    void live(std::map<block_index_t, std::vector<BlockAddress>> &live);

    int32_t number_of_chains(block_index_t first, block_index_t last, BlockType type);

};

}

#endif
