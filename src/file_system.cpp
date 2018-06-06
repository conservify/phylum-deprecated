#include "confs/file_system.h"

namespace confs {

template<typename NodeType>
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

bool FileSystem::initialize(bool wipe) {
    if (!storage_->open()) {
        return false;
    }

    return open(wipe);
}

bool FileSystem::open(bool wipe) {
    if (wipe || !sbm_.locate()) {
        if (!format()) {
            return false;
        }
    }

    tree_addr_ = find_tree();

    return tree_addr_.valid();
}

bool FileSystem::exists(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = make_key(2, id);

    return tc.find(key) != 0;
}

OpenFile FileSystem::open(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = make_key(2, id);

    auto existing = tc.find(key);
    if (existing == 0) {
        tc.add(key, 1);
    }

    return { *this, false };
}

bool FileSystem::close() {
    return storage_->close();
}

bool FileSystem::touch() {
    TreeContext<NodeType> tc{ *this };
    tc.touch();
    return true;
}

BlockAddress FileSystem::find_tree() {
    TreeContext<NodeType> tc{ *this };

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

bool FileSystem::format() {
    if (!sbm_.create()) {
        return false;
    }
    if (!sbm_.locate()) {
        return false;
    }

    return touch();
}

OpenFile::OpenFile(FileSystem &fs, bool readonly) : fs_(&fs), readonly_(readonly) {
}

size_t OpenFile::write(const void *ptr, size_t size) {
    return 0;
}

void OpenFile::close() {
}

}
