#ifndef __PHYLUM_UTILITIES_H_INCLUDED
#define __PHYLUM_UTILITIES_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <iomanip>
#include <string>

#include <phylum/tree.h>
#include <phylum/private.h>
#include <phylum/backend.h>

std::map<uint64_t, uint64_t> random_data();

namespace phylum {

class BlockHelper {
private:
    StorageBackend *storage_;

public:
    BlockHelper(StorageBackend &storage) : storage_(&storage) {
    }

public:
    bool is_type(block_index_t block, BlockType type);

};

}

#endif
