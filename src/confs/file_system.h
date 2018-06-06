#ifndef __CONFS_FILE_SYSTEM_H_INCLUDED
#define __CONFS_FILE_SYSTEM_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/block_alloc.h"
#include "confs/super_block.h"
#include "confs/crc.h"
#include "confs/inodes.h"
#include "confs/backend_nodes.h"

namespace confs {

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

public:
    StorageBackend &storage() {
        return *storage_;
    }

private:
    struct TreeContext {
    public:
        FileSystem &fs;
        NodeSerializer<NodeType> serializer;
        MemoryConstrainedNodeCache<NodeType, 8> cache;
        PersistedTree<NodeType> tree;
        BlockAddress new_head;

    public:
        TreeContext(FileSystem &fs) :
            fs(fs), cache(fs.nodes_), tree(cache) {

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
        size_t write(const void *ptr, size_t size) {
            return 0;
        }

        void close() {
        }
    };

public:
    bool initialize(bool wipe = false) {
        if (!storage_->open()) {
            return false;
        }

        return open(wipe);
    }

    bool open(bool wipe = false) {
        if (wipe || !sbm_.locate()) {
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
        return storage_->close();
    }

private:
    bool touch() {
        TreeContext tc{ *this };
        tc.touch();
        return true;
    }

    BlockAddress find_tree() {
        TreeContext tc{ *this };

        auto tree_block = sbm_.tree_block();
        assert(tree_block != BLOCK_INDEX_INVALID);

        auto &geometry = storage_->geometry();
        auto required = tc.serializer.size(true);
        auto iter = BlockAddress{ tree_block, 0 };
        auto found = BlockAddress{ };

        // We could compare TreeHead timestamps, though we always append.
        while (iter.remaining_in_block(geometry) > required) {
            TreeHead head;
            NodeType node;

            if (iter.beginning_of_block()) {
                TreeBlockHeader header;
                if (!storage_->read(iter, &header, sizeof(TreeBlockHeader))) {
                    return { };
                }

                if (!header.valid()) {
                    return found;
                }

                iter.add(SectorSize);
            }
            else {
                if (nodes_.deserialize(iter, &node, &head)) {
                    found = iter;
                    iter.add(required);
                }
                else {
                    break;
                }
            }
        }

        return found;
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

};

}

#endif
