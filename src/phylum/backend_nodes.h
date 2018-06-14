#ifndef __PHYLUM_BACKEND_NODES_H_INCLUDED
#define __PHYLUM_BACKEND_NODES_H_INCLUDED

#include "phylum/persisted_tree.h"
#include "phylum/node_serializer.h"
#include "phylum/layout.h"

namespace phylum {

struct TreeBlockHead {
    BlockHead block;

    TreeBlockHead(BlockType type) : block(type) {
    }

    void fill() {
        block.magic.fill();
        block.age = 0;
        block.timestamp = 0;
    }

    bool valid() const {
        return block.valid();
    }
};

struct TreeBlockTail {
    BlockTail block;
};

inline std::ostream& operator<<(std::ostream& os, const TreeBlockHead &h) {
    return os << "TreeBlock<" << h.block << ">";
}

static inline BlockLayout<TreeBlockHead, TreeBlockTail> get_layout(StorageBackend &storage,
              BlockAllocator &allocator, BlockAddress address, BlockType type) {
    return { storage, allocator, address, type };
}

struct TreeStorageState {
    BlockAddress index;
    BlockAddress leaf;
};

template<typename NODE>
class StorageBackendNodeStorage : public NodeStorage<NODE, BlockAddress> {
public:
    using NodeType = NODE;
    using SerializerType = NodeSerializer<NodeType>;

private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    BlockAddress index_;
    BlockAddress leaf_;

public:
    StorageBackendNodeStorage(StorageBackend &storage, BlockAllocator &allocator)
        : storage_(&storage), allocator_(&allocator) {
    }

public:
    TreeStorageState state() {
        return TreeStorageState{ index_, leaf_ };
    }

    void state(TreeStorageState state) {
        index_ = state.index;
        leaf_ = state.leaf;
    }

    bool recreate() {
        leaf_ = BlockAddress{ };
        index_ = BlockAddress{ };
        return true;
    }

    bool deserialize(BlockAddress addr, NodeType *node, TreeHead *head) {
        SerializerType serializer;

        auto required = serializer.size(head != nullptr);

        uint8_t buffer[SerializerType::HeadNodeSize];
        if (!storage_->read(addr, buffer, required)) {
            return false;
        }

        if (!serializer.deserialize(buffer, node, head)) {
            return false;
        }

        return true;
    }

    BlockAddress serialize(BlockAddress addr, const NodeType *node, const TreeHead *head) {
        SerializerType serializer;

        auto &location = node->depth == 0 ? leaf_ : index_;
        auto type = node->depth == 0 ? BlockType::Leaf : BlockType::Index;
        auto required = serializer.size(head != nullptr);
        auto layout = get_layout(*storage_, *allocator_, location, type);

        auto address = layout.find_available(required);
        if (!address.valid()) {
            return { };
        }

        location = address;
        location.add(required);

        uint8_t buffer[SerializerType::HeadNodeSize];
        if (!serializer.serialize(buffer, node, head)) {
            return { };
        }

        if (!storage_->write(address, buffer, required)) {
            return { };
        }

        return address;
    }

    BlockAddress find_head(block_index_t block) {
        SerializerType serializer;

        assert(block != BLOCK_INDEX_INVALID);

        auto layout = get_layout(*storage_, *allocator_, BlockAddress{ block, 0 }, BlockType::Error);
        auto required = serializer.size(true);

        auto fn = [&](StorageBackend &storage, BlockAddress& address) -> bool {
            TreeHead head;
            NodeType node;

            if (deserialize(address, &node, &head)) {
                return true;
            }

            return false;
        };

        if (!layout.find_tail_entry(block, required, fn)) {
            return { };
        }

        auto address = layout.address();

        return address;
    }

};

}

#endif
