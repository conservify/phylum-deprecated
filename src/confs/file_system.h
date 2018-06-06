#ifndef __CONFS_FILE_SYSTEM_H_INCLUDED
#define __CONFS_FILE_SYSTEM_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/block_alloc.h"
#include "confs/super_block.h"
#include "confs/crc.h"
#include "confs/inodes.h"
#include "confs/backend_nodes.h"

namespace confs {

class FileSystem;

class OpenFile {
private:
    FileSystem *fs_;
    uint32_t id_;
    bool readonly_{ false };
    uint8_t buffer_[SectorSize];

public:
    OpenFile(FileSystem &fs, bool readonly);

public:
    size_t write(const void *ptr, size_t size);
    void close();

};

class FileSystem {
private:
    using NodeType = Node<INodeKey, uint64_t, BlockAddress, 6, 6>;

    StorageBackend *storage_;
    BlockAllocator allocator_;
    SuperBlockManager sbm_;
    StorageBackendNodeStorage<NodeType> nodes_;
    BlockAddress tree_addr_;

public:
    FileSystem(StorageBackend &storage) : storage_(&storage), allocator_(storage), sbm_{ storage, allocator_ }, nodes_{ storage, allocator_ } {
    }

    template<typename NodeType>
    friend struct TreeContext;

public:
    StorageBackend &storage() {
        return *storage_;
    }

public:
    bool initialize(bool wipe = false);
    bool open(bool wipe = false);
    bool exists(const char *name);
    OpenFile open(const char *name);
    bool close();

private:
    bool touch();
    BlockAddress find_tree();
    bool format();

};

}

#endif
