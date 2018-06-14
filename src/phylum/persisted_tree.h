#ifndef __PHYLUM_PERSISTED_TREE_H_INCLUDED
#define __PHYLUM_PERSISTED_TREE_H_INCLUDED

#include "phylum/private.h"
#include "phylum/keys.h"

namespace phylum {

using DepthType = uint8_t;
using IndexType = uint8_t;

struct TreeHead {
    timestamp_t timestamp;
};

template<typename KEY, typename VALUE, typename ADDRESS, size_t N, size_t M>
class Node;

template<typename KEY, typename VALUE, typename ADDRESS, size_t N, size_t M>
struct PersistedTreeSchema {
    using KeyType = KEY;
    using ValueType = VALUE;
    using AddressType = ADDRESS;
    using IndexType = uint8_t;
    using DepthType = uint8_t;
    using NodeType = Node<KEY, VALUE, ADDRESS, N, M>;
    static constexpr size_t Keys = N;
    static constexpr size_t Values = M;
    static constexpr size_t Children = N + 1;
};

template<typename NODE, typename ADDRESS>
class NodeStorage {
public:
    virtual bool recreate() = 0;
    virtual bool deserialize(ADDRESS addr, NODE *node, TreeHead *head) = 0;
    virtual ADDRESS serialize(ADDRESS addr, const NODE *node, const TreeHead *head) = 0;

};

template<typename ADDRESS>
class NodeRef {
private:
    IndexType index_{ 0xff };
    ADDRESS address_;

public:
    NodeRef() {
    }

    NodeRef(uint8_t index) : index_(index) {
    }

    NodeRef(ADDRESS address) : address_(address) {
    }

    NodeRef(const NodeRef &ref) : index_(ref.index_), address_(ref.address_) {
    }

    NodeRef& operator=(NodeRef other) {
        index_ = other.index_;
        address_ = other.address_;
        return *this;
    }

    template<typename NADDRESS>
    friend std::ostream& operator<<(std::ostream& os, const NodeRef<NADDRESS> &n);

    template<typename KEY, typename VALUE, typename NADDRESS, size_t N, size_t M>
    friend std::ostream& operator<<(std::ostream& os, const Node<KEY, VALUE, NADDRESS, N, M> &n);

public:
    ADDRESS address() const {
        return address_;
    }

    void address(ADDRESS address) {
        address_ = address;
        assert(address_.valid());
    }

    IndexType index() const {
        return index_;
    }

    void index(IndexType index) {
        index_ = index;
    }

    bool valid() const {
        return address_.valid();
    }

    void clear() {
        address_.invalid();
        index_ = 0xff;
    }

};

template<typename KEY, typename VALUE, typename ADDRESS, size_t N, size_t M>
class Node {
public:
    static constexpr size_t InnerSize = N;
    static constexpr size_t LeafSize = M;
    using NodeRefType = NodeRef<ADDRESS>;
    using KeyType = KEY;
    using ValueType = VALUE;
    using AddressType = ADDRESS;

public:
    DepthType depth;
    IndexType number_keys;
    KEY keys[N];
    union data_t {
        VALUE values[M];
        NodeRefType children[N + 1];

        data_t() {
        }
    };

    data_t d;

public:
    Node() {
    }

public:
    void clear() {
        depth = 0;
        number_keys = 0;
        for (auto i = (IndexType)0; i < N; ++i) {
            keys[i] = 0;
            d.values[i] = 0;
        }
        for (auto &ref : d.children) {
            ref.clear();
        }
    }

    bool empty() const {
        return number_keys == 0;
    }

};

template<typename KEY, typename VALUE, typename ADDRESS, size_t N, size_t M>
std::ostream& operator<<(std::ostream& os, const Node<KEY, VALUE, ADDRESS, N, M> &n);

template<typename NODE>
class NodeCache {
public:
    using NodeType = NODE;
    using NodeRefType = NodeRef<typename NODE::AddressType>;

public:
    virtual NodeRefType allocate() = 0;
    virtual NodeRefType load(NodeRefType ref, bool head = false) = 0;
    virtual void unload(NodeRefType ref) = 0;
    virtual NodeType *resolve(NodeRefType ref) = 0;
    virtual NodeRefType flush() = 0;
    virtual NodeRefType flush(NodeRefType ref, bool head) = 0;
    virtual void clear() = 0;
    virtual void recreate() = 0;
};

template<typename NodeRefType, typename NodeType>
class PersistedTreeVisitor {
public:
    virtual void visit(NodeRefType nref, NodeType *node) = 0;

};

/**
 * TODO: Create RIAA that releases unmodified nodes from the cache.
 * This implemantion appears to have originally come from the following location:
 * https://en.wikibooks.org/wiki/Algorithm_Implementation/Trees/B%2B_tree
 * It has been modified from its original form somewhat.
 */
template<typename NODE, size_t N = NODE::InnerSize, size_t M = NODE::LeafSize>
class PersistedTree {
public:
    using KEY = typename NODE::KeyType;
    using VALUE = typename NODE::ValueType;
    using ADDRESS = typename NODE::AddressType;
    using NodeType = Node<KEY, VALUE, ADDRESS, N, M>;
    using NodeCacheType = NodeCache<NodeType>;
    using NodeRefType = NodeRef<ADDRESS>;

private:
    NodeCacheType *nodes_;
    NodeRefType ref_;

public:
    PersistedTree(NodeCacheType &nodes, ADDRESS address = ADDRESS()) : nodes_(&nodes), ref_(address) {
    }

public:
    void head(ADDRESS address) {
        ref_ = { address };
    }

    VALUE find(KEY key) {
        create_if_necessary();

        assert(ref_.valid());

        auto nref = nodes_->load(ref_, true);
        auto node = nodes_->resolve(nref);
        auto d = node->depth;
        while (d-- != 0) {
            auto index = Keys::inner_position_for(key, node->keys, node->number_keys);
            assert(index < node->number_keys + 1);

            nref = load_child(node, index);
            node = nodes_->resolve(nref);
        }

        VALUE value = 0;

        auto index = Keys::leaf_position_for(key, node->keys, node->number_keys);
        if (index < node->number_keys && node->keys[index] == key) {
            value = node->d.values[index];
        }

        nodes_->clear();

        return value;
    }

    // Finds the LAST item that is < key. That is, the next item in the tree is not < key, but this
    // item is. If we were to insert key into the tree, it would go after this item. This is weird,
    // but is easier than implementing iterators. In STL terms, this would be "lower_bound(key)--"
    // WARNING: This does *not* work when values are deleted. Thankfully, TPC-C does not use deletes.
    bool find_less_then(const KEY &key, VALUE *value = 0, KEY *found = 0) {
        create_if_necessary();

        assert(ref_.valid());

        auto nref = nodes_->load(ref_, true);
        auto node = nodes_->resolve(nref);
        auto d = node->depth;

        while (d-- != 0) {
            auto index = Keys::inner_position_for(key, node->keys, node->number_keys);
            // We need to rewind in the case where they are equal
            if (index > 0 && key == node->keys[index - 1]) {
                index -= 1;
            }
            assert(index == 0 || node->keys[index - 1] < key);

            nref = load_child(node, index);
            node = nodes_->resolve(nref);
        }

        auto index = Keys::leaf_position_for(key, node->keys, node->number_keys);
        if (index <= node->number_keys) {
            index -= 1;
            if (index < node->number_keys && key == node->keys[index]) {
                index -= 1;
            }

            if (index < node->number_keys) {
                assert(node->keys[index] < key);
                if (node->d.values[index]) {
                    if (value != nullptr) {
                        *value = node->d.values[index];
                    }
                    if (found != nullptr) {
                        *found = node->keys[index];
                    }
                    nodes_->clear();
                    return true;
                }
                else {
                    assert(false);
                    /*
                    // This is a deleted key! Try again with the new key value
                    // HACK: This is because this implementation doesn't do deletes correctly. However,
                    // this makes it work. We need this for TPC-C undo. The solution is to use a
                    // more complete b-tree implementation.
                    nodes_->clear();
                    return find_last_less_then(node->keys[index], value, found);
                    */
                }
            }
        }

        nodes_->clear();
        return false;
    }

    ADDRESS add(KEY key, VALUE value) {
        create_if_necessary();

        assert(ref_.valid());

        auto nref = nodes_->load(ref_, true);
        auto node = nodes_->resolve(nref);

        SplitOutcome split_outcome;
        if (node->depth == 0) {
            split_outcome = leaf_insert(nref, key, value);
        }
        else {
            split_outcome = inner_insert(nref, node->depth, key, value);
        }

        if (split_outcome) {
            #ifdef PHYLUM_PERSISTED_TREE_LOGGING
            sdebug() << "New Root (" << (size_t)node->depth << " " << split_outcome.key << ")" << std::endl;
            #endif
            auto new_nref = nodes_->allocate();
            auto new_node = nodes_->resolve(new_nref);
            new_node->depth = node->depth + 1;
            new_node->number_keys = 1;
            new_node->keys[0] = split_outcome.key;
            new_node->d.children[0] = split_outcome.left;
            new_node->d.children[1] = split_outcome.right;
        }

        ref_ = nodes_->flush();

        return address();
    }

    ADDRESS address() {
        return ref_.address();
    }

    bool remove(const KEY key) {
        auto nref = nodes_->load(ref_, true);
        auto node = nodes_->resolve(nref);

        auto d = node->depth;
        while (d-- != 0) {
            auto index = Keys::inner_position_for(key, node->keys, node->number_keys);
            assert(index < node->number_keys + 1);

            nref = load_child(node, index);
            node = nodes_->resolve(nref);
        }

        auto index = Keys::leaf_position_for(key, node->keys, node->number_keys);
        if (node->keys[index] == key) {
            node->d.values[index] = 0;
            ref_ = nodes_->flush();
            return true;
        }
        else {
            nodes_->clear();
            return false;
        }
    }

    ADDRESS create_if_necessary() {
        if (ref_.valid()) {
            return ref_.address();
        }

        auto nref = nodes_->allocate();
        auto node = nodes_->resolve(nref);
        node->clear();
        ref_ = nodes_->flush();

        return ref_.address();
    }

    using PersistedTreeVisitorType = PersistedTreeVisitor<NodeRefType, NodeType>;

    void accept(NodeRefType ref, PersistedTreeVisitorType &visitor, bool head = false) {
        auto nref = nodes_->load(ref, head);
        auto node = nodes_->resolve(nref);

        visitor.visit(nref, node);

        if (node->depth > 0) {
            for (auto i = 0; i < node->number_keys + 1; ++i) {
                if (node->d.children[i].valid()) {
                    accept(node->d.children[i], visitor);
                }
            }
        }

        nodes_->unload(nref);
    }

    void accept(PersistedTreeVisitorType &visitor) {
        create_if_necessary();

        accept(ref_, visitor, true);
    }

    ADDRESS recreate(NodeRefType ref, bool head = false) {
        auto nref = nodes_->load(ref, head);
        auto node = nodes_->resolve(nref);

        if (node->depth > 0) {
            for (auto i = 0; i < node->number_keys + 1; ++i) {
                if (node->d.children[i].valid()) {
                    node->d.children[i] = recreate(node->d.children[i]);
                }
            }
        }

        auto new_ref = nodes_->flush(nref, head);

        nodes_->unload(nref);

        return new_ref.address();
    }

    ADDRESS recreate() {
        create_if_necessary();

        nodes_->recreate();

        auto new_head = recreate(ref_, true);

        ref_.address(new_head);

        return new_head;
    }

private:
    struct SplitOutcome {
        KEY key;
        IndexType threshold;
        NodeRefType left;
        NodeRefType right;

        operator bool() {
            return key != 0;
        }
    };

    NodeRefType load_child(NodeType *node, IndexType i) {
        return node->d.children[i] = nodes_->load(node->d.children[i]);
    }

    SplitOutcome leaf_insert(NodeRefType nref, KEY key, VALUE value) {
        auto node = nodes_->resolve(nref);
 
        assert(node->number_keys <= M);

        auto i = Keys::leaf_position_for(key, node->keys, node->number_keys);
        if (node->number_keys == M) {
            auto threshold = (IndexType)((M + 1) / 2);
            auto new_nref = nodes_->allocate();
            auto new_sibling = nodes_->resolve(new_nref);

            new_sibling->depth = node->depth;
            new_sibling->number_keys = node->number_keys - threshold;
            for (auto j = 0; j < new_sibling->number_keys; ++j) {
                new_sibling->keys[j] = node->keys[threshold + j];
                new_sibling->d.values[j] = node->d.values[threshold + j];
            }
            node->number_keys = threshold;

            if (i < threshold) {
                leaf_insert_nonfull(nref, i, key, value);
            }
            else {
                leaf_insert_nonfull(new_nref, i - threshold, key, value);
            }

            return { new_sibling->keys[0], threshold, nref, new_nref };
        }
        else {
            leaf_insert_nonfull(nref, i, key, value);
        }

        return { };
    }

    void leaf_insert_nonfull(NodeRefType nref, IndexType index, KEY key, VALUE value) {
        auto node = nodes_->resolve(nref);

        assert(node->depth == 0);
        assert(node->number_keys < M);
        assert(index < M);
        assert(index <= node->number_keys);

        #ifdef PHYLUM_PERSISTED_TREE_LOGGING
        sdebug() << "Add " << key << " to " << *node << " index=" << (size_t)index <<
            " keys=" << (size_t)node->number_keys << std::endl;
        #endif

        if (node->keys[index] == key) {
            // We are inserting a duplicate value. Simply overwrite the old one
            node->d.values[index] = value;
        }
        else {
            for (auto i = node->number_keys; i > index; --i) {
                node->keys[i] = node->keys[i - 1];
                node->d.values[i] = node->d.values[i - 1];
            }
            node->number_keys++;
            node->keys[index] = key;
            node->d.values[index] = value;
        }
    }

    SplitOutcome inner_insert(NodeRefType nref, DepthType level, KEY key, VALUE value) {
        assert(level > 0);

        auto node = nodes_->resolve(nref);

        if (node->number_keys == N) {
            auto threshold = (IndexType)((N + 1) / 2);
            auto new_nref = nodes_->allocate();
            auto new_sibling = nodes_->resolve(new_nref);

            new_sibling->depth = node->depth;
            new_sibling->number_keys = node->number_keys - threshold;
            for (auto i = 0; i < new_sibling->number_keys; ++i) {
                new_sibling->keys[i] = node->keys[threshold + i];
                new_sibling->d.children[i] = node->d.children[threshold + i];
            }
            new_sibling->d.children[new_sibling->number_keys] = node->d.children[node->number_keys];

            node->number_keys = threshold - 1;

            auto threshold_key = node->keys[threshold - 1];

            if (key < threshold_key) {
                inner_insert_nonfull(nref, level, key, value);
            }
            else {
                inner_insert_nonfull(new_nref, level, key, value);
            }

            return SplitOutcome { threshold_key, threshold, nref, new_nref };
        }
        else {
            inner_insert_nonfull(nref, level, key, value);
        }

        return { };
    }

    void inner_insert_nonfull(NodeRefType nref, DepthType level, KEY &key, VALUE &value) {
        auto node = nodes_->resolve(nref);

        assert(node->number_keys < N);
        assert(level != 0);
        assert(node->depth > 0);
        assert(node->number_keys > 0);

        SplitOutcome ins;

        auto index = Keys::inner_position_for(key, node->keys, node->number_keys);

        auto child = load_child(node, index);
        if (level - 1 == 0) {
            ins = leaf_insert(child, key, value);
        }
        else {
            ins = inner_insert(child, level - 1, key, value);
        }

        if (ins) {
            if (index == node->number_keys) {
                node->keys[index] = ins.key;
                node->d.children[index] = ins.left;
                node->d.children[index + 1] = ins.right;
                node->number_keys++;
            }
            else {
                node->d.children[node->number_keys + 1] = node->d.children[node->number_keys];
                for (auto i = node->number_keys; i != index; --i) {
                    node->d.children[i] = node->d.children[i - 1];
                    node->keys[i] = node->keys[i - 1];
                }
                node->number_keys++;
                node->d.children[index] = ins.left;
                node->d.children[index + 1] = ins.right;
                node->keys[index] = ins.key;
            }
        }
    }

};

template<typename KEY, typename VALUE, typename ADDRESS, size_t N, size_t M>
std::ostream& operator<<(std::ostream& os, const Node<KEY, VALUE, ADDRESS, N, M> &n) {
    os << "NODE<" << (size_t)n.depth << " " << (size_t)n.number_keys << "";

    if (n.depth == 0) {
        for (auto i = 0; i < n.number_keys; ++i) {
            os << " " << n.keys[i] << "=" << n.d.values[i];
        }
    }
    else {
        for (auto i = 0; i < n.number_keys + 1; ++i) {
            os << " " << n.keys[i] << "=Child(#" << (size_t)n.d.children[i].index() << " " << n.d.children[i].address() << ")";
        }
    }

    os << ">";

    return os;
}

template<typename ADDRESS>
std::ostream& operator<<(std::ostream& os, const NodeRef<ADDRESS> &n) {
    return os << "Ref<#" << (size_t)n.index_ << " addr=" << n.address() << ">";
}

}

#endif
