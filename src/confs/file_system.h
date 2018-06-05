#ifndef __CONFS_FILE_SYSTEM_H_INCLUDED
#define __CONFS_FILE_SYSTEM_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/block_alloc.h"
#include "confs/super_block.h"
#include "confs/crc.h"
#include "confs/inodes.h"

namespace confs {

template<typename StorageBackendType>
class FileSystem {
private:
    using NodeType = Node<INodeKey, uint64_t, confs_sector_addr_t, 6, 6>;

    confs_geometry_t geometry_{ 1024, 4, 4, 512 };
    StorageBackendType storage_;
    BlockAllocator allocator_{ storage_ };
    SuperBlockManager sbm_{ storage_, allocator_ };
    confs_sector_addr_t tree_addr_;

private:
    struct TreeContext {
    private:
        FileSystem &fs;
        StorageBackendNodeStorage<NodeType> nodes;
        MemoryConstrainedNodeCache<NodeType, 8> cache;
        PersistedTree<NodeType> tree;
        confs_sector_addr_t new_head;

    public:
        TreeContext(FileSystem &fs, StorageBackendType &storage, BlockAllocator &allocator) :
            fs(fs), nodes(storage, allocator), cache(nodes), tree(cache) {

            if (fs.tree_addr_.valid()) {
                tree.head(fs.tree_addr_);
            }
        }

        ~TreeContext() {
            if (new_head.valid()) {
                fs.sbm_.save(new_head.block);
                fs.tree_addr_ = new_head;
            }
        }

    public:
        void add(INodeKey key, uint64_t value) {
            new_head = tree.add(key, value);
        }

        uint64_t find(INodeKey key) {
            return tree.find(key);
        }
    };

public:
    class OpenFile {
    private:
        uint32_t id_;
        bool mutable_{ false };
        uint8_t buffer_[512];

    public:
    };

public:
    bool initialize() {
        if (!storage_.initialize(geometry_)) {
            return false;
        }
        if (!storage_.open()) {
            return false;
        }
        if (!sbm_.create()) {
            return false;
        }
        if (!sbm_.locate()) {
            return false;
        }

        tree_addr_ = find_tree();

        return true;
    }

    confs_sector_addr_t find_tree() {
        auto tree_block = sbm_.tree_block();

        sdebug << "Tree Block: " << tree_block << std::endl;

        return { };
    }

    bool exists(const char *name) {
        TreeContext tc{ *this, storage_, allocator_ };

        auto id = crc32_checksum((uint8_t *)name, strlen(name));
        auto key = make_key(2, id);

        return tc.find(key) != 0;
    }

    OpenFile open(const char *name) {
        TreeContext tc{ *this, storage_, allocator_ };

        auto id = crc32_checksum((uint8_t *)name, strlen(name));
        auto key = make_key(2, id);

        auto existing = tc.find(key);
        if (existing == 0) {
            tc.add(key, 1);
        }

        return { };
    }

    bool close() {
        return storage_.close();
    }
};

}

#endif
