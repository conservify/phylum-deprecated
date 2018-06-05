#ifndef __CONFS_FILE_SYSTEM_H_INCLUDED
#define __CONFS_FILE_SYSTEM_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/block_alloc.h"
#include "confs/super_block.h"
#include "confs/crc.h"
#include "confs/inodes.h"
#include "confs/backend_nodes.h"

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
    public:
        FileSystem &fs;
        StorageBackendNodeStorage<NodeType> nodes;
        MemoryConstrainedNodeCache<NodeType, 8> cache;
        PersistedTree<NodeType> tree;
        confs_sector_addr_t new_head;

    public:
        TreeContext(FileSystem &fs) :
            fs(fs), nodes(fs.storage_, fs.allocator_), cache(nodes), tree(cache) {

            if (fs.tree_addr_.valid()) {
                tree.head(fs.tree_addr_);
            }
        }

        ~TreeContext() {
            flush();
        }

    public:
        void add(INodeKey key, uint64_t value) {
            new_head = tree.add(key, value);
        }

        uint64_t find(INodeKey key) {
            return tree.find(key);
        }

        void touch() {
            new_head = tree.create_if_necessary();
        }

        bool flush() {
            if (new_head.valid()) {
                fs.sbm_.save(new_head.block);
                fs.tree_addr_ = new_head;
                new_head.invalid();
                return true;
            }
            return true;
        }
    };

public:
    class OpenFile {
    private:
        uint32_t id_;
        bool mutable_{ false };
        uint8_t buffer_[512];

    public:
        OpenFile() {
        }

    public:
        void close() {
        }
    };

public:
    bool initialize() {
        if (!storage_.initialize(geometry_)) {
            return false;
        }

        if (!storage_.open()) {
            return false;
        }

        if (!sbm_.locate()) {
            if (!format()) {
                return false;
            }
        }

        tree_addr_ = find_tree();

        return tree_addr_.valid();
    }

    bool exists(const char *name) {
        TreeContext tc{ *this };

        auto id = crc32_checksum((uint8_t *)name, strlen(name));
        auto key = make_key(2, id);

        return tc.find(key) != 0;
    }

    OpenFile open(const char *name) {
        TreeContext tc{ *this };

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

private:
    bool touch() {
        TreeContext tc{ *this };
        tc.touch();
        return true;
    }

    bool format() {
        if (!sbm_.create()) {
            return false;
        }
        if (!sbm_.locate()) {
            return false;
        }

        return touch();
    }

    confs_sector_addr_t find_tree() {
        TreeContext tc{ *this };

        auto tree_block = sbm_.tree_block();
        auto addr = confs_sector_addr_t{ tree_block, 0 };
        auto found = confs_sector_addr_t{ };

        while (addr.sector < storage_.geometry().sectors_per_block()) {
            // sdebug << "Finding: " << addr << std::endl;

            TreeHead head;
            NodeType node;
            if (tc.nodes.deserialize(addr, &node, &head)) {
                found = addr;
            }
            else {
                break;
            }

            addr.sector++;
        }

        return found;
    }
};

}

#endif
