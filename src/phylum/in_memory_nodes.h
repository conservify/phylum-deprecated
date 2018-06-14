#ifndef __PHYLUM_IN_MEMORY_NODES_H_INCLUDED
#define __PHYLUM_IN_MEMORY_NODES_H_INCLUDED

#include "phylum/persisted_tree.h"
#include "phylum/node_serializer.h"

namespace phylum {

template<typename NODE, typename ADDRESS = typename NODE::AddressType>
class InMemoryNodeStorage : public NodeStorage<NODE, ADDRESS> {
public:
    using NodeType = NODE;
    using SerializerType = NodeSerializer<NodeType>;

private:
    void *ptr_;
    size_t size_;
    size_t position_;

public:
    InMemoryNodeStorage(size_t size) : ptr_(nullptr), size_(size), position_(0) {
        ptr_ = malloc(size);
    }

    ~InMemoryNodeStorage() {
        if (ptr_ != nullptr) {
            free(ptr_);
            ptr_ = nullptr;
        }
    }

public:
    bool recreate() override {
        return true;
    }

    virtual bool deserialize(ADDRESS addr, NodeType *node, TreeHead *head) override {
        SerializerType serializer;
        return serializer.deserialize(lookup(addr), node, head);
    }

    virtual ADDRESS serialize(ADDRESS addr, const NodeType *node, const TreeHead *head) override {
        SerializerType serializer;

        if (!addr.valid()) {
            addr = allocate(serializer.size(head));
        }

        if (!serializer.serialize(lookup(addr), node, head)) {
            return { };
        }

        return addr;
    }

private:
    void *lookup(BlockAddress addr) {
        return (uint8_t *)ptr_ + addr.position;
    }

    BlockAddress allocate(size_t size) {
        auto addr = BlockAddress{ 0, (uint32_t)position_ };
        assert(position_ + size < size_);
        position_ += size;
        return addr;
    }
};

}

#endif
