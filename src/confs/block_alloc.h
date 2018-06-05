#ifndef __CONFS_BLOCK_ALLOC_H_INCLUDED
#define __CONFS_BLOCK_ALLOC_H_INCLUDED

#include <confs/confs.h>
#include <confs/private.h>
#include <confs/backend.h>

#include <queue>

namespace confs {

class BlockAllocator {
private:
    bool initialized_{ false };
    std::queue<block_index_t> free_;
    StorageBackend *storage_;

public:
    BlockAllocator(StorageBackend &storage);

public:
    block_index_t allocate();
    void free(block_index_t block);

};

}

#endif
