#ifndef __PHYLUM_STACK_NODE_CACHE_H_INCLUDED
#define __PHYLUM_STACK_NODE_CACHE_H_INCLUDED

#include "persisted_tree.h"

namespace phylum {

template<typename NODE, size_t SIZE>
class MemoryConstrainedNodeCache : public NodeCache<NODE> {
public:
    using NodeType = NODE;
    using NodeRefType = NodeRef<typename NODE::AddressType>;
    using NodeStorageType = NodeStorage<NodeType, typename NODE::AddressType>;

private:
    NodeStorageType *storage_;
    NodeType nodes_[SIZE];
    NodeRefType pending_[SIZE];
    IndexType index_{ 0 };
    TreeHead information_{ 0 };

public:
    MemoryConstrainedNodeCache(NodeStorageType &storage) : storage_(&storage) {
        clear();
    }

public:
    virtual NodeType *resolve(NodeRefType ref) override {
        assert(ref.index() != 0xff);
        return &nodes_[ref.index()];
    }

    virtual NodeRefType allocate() override {
        assert(index_ != SIZE);
        auto i = index_++;
        auto ref = NodeRefType { i };
        pending_[i] = ref;
        return ref;
    }

    virtual NodeRefType load(NodeRefType ref, bool head = false) override {
        assert(ref.address().valid());

        auto new_ref = allocate();
        ref.index(new_ref.index());
        pending_[ref.index()] = ref;

        auto node = &nodes_[ref.index()];

        storage_->deserialize(ref.address(), node, head ? &information_ : nullptr);

        #ifdef PHYLUM_PERSISTED_TREE_LOGGING
        sdebug() << "Load: " << ref.address() << " " << *node << std::endl;
        #endif

        return ref;
    }

    virtual void unload(NodeRefType ref) override {
        assert(ref.address().valid());
        assert(index_ - 1 == ref.index());

        index_--;

        nodes_[ref.index()].clear();
    }

    virtual NodeRefType flush() override {
        #ifdef PHYLUM_PERSISTED_TREE_LOGGING
        sdebug() << "Flushing Node Cache: " << (size_t)index_ << std::endl;
        #endif

        if (index_ == 0) {
            return NodeRefType{ };
        }

        IndexType head_index = 0;
        DepthType head_depth = nodes_[pending_[head_index].index()].depth;

        for (auto i = (IndexType)1; i < index_; ++i) {
            if (nodes_[pending_[i].index()].depth > head_depth) {
                head_depth = nodes_[pending_[i].index()].depth;
                head_index = pending_[i].index();
            }
        }

        auto head = flush(pending_[head_index], true);

        clear();

        return head;
    }

    virtual NodeRefType flush(NodeRefType ref, bool head = false) override {
        assert(ref.index() != 0xff);

        if (head) {
            information_.timestamp++;
        }

        auto node = &nodes_[ref.index()];
        if (node->depth > 0) {
            for (auto i = 0; i <= node->number_keys; ++i) {
                if (node->d.children[i].index() != 0xff) {
                    node->d.children[i] = flush(node->d.children[i]);
                }
            }
        }

        auto new_address = storage_->serialize(ref.address(), node, head ? &information_ : nullptr);

        ref.address(new_address);

        #ifdef PHYLUM_PERSISTED_TREE_LOGGING
        sdebug() << "   " << " W(#" << (int32_t)ref.index() << " " << ref.address() << " = " << *node << ")" << std::endl;
        #endif

        return ref;
    }

    virtual void clear() override {
        index_ = 0;
        for (auto &node : nodes_) {
            node.clear();
        }
    }

    virtual void recreate() override {
        storage_->recreate();
    }

};

}

#endif
