#ifndef __CONFS_TREE_H_INCLUDED
#define __CONFS_TREE_H_INCLUDED

#include <functional>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>

namespace confs {

class Keys {
public:
    // Returns the position where 'key' should be inserted in a leaf node
    // that has the given keys.
    // NOTE: These and the following methods do a simple linear search, which is
    // just fine for N or M < 100. Any large and a Binary Search is better.
    template<typename KEY>
    static unsigned leaf_position_for(const KEY &key, const KEY *keys, unsigned number_keys) {
        uint8_t k = 0;
        while ((k < number_keys) && (keys[k] < key)) {
            ++k;
        }
        assert(k <= number_keys);
        return k;
    }

    // Returns the position where 'key' should be inserted in an inner node
    // that has the given keys.
    template<typename KEY>
    static inline uint8_t inner_position_for(const KEY &key, const KEY *keys, unsigned number_keys) {
        uint8_t k = 0;
        while ((k < number_keys) && ((keys[k] < key) || (keys[k] == key))) {
            ++k;
        }
        return k;
    }

};

// This implemantion appears to have originally come from the following location:
// https://en.wikibooks.org/wiki/Algorithm_Implementation/Trees/B%2B_tree
// It has been modified from its original form somewhat.
template <typename KEY, typename VALUE, unsigned N, unsigned M>
class BPlusTree {
public:
    static constexpr size_t InnerWidth{ N };
    static constexpr size_t ChildrenWidth{ N + 1 };
    static constexpr size_t LeafWidth{ M };

    using number_keys_t = unsigned;
    using KeyType = KEY;
    using ValueType = VALUE;

    struct Node;
    struct InnerNode;
    struct LeafNode;

    class BPlusTreeVisitor {
    public:
        virtual void visit(InnerNode *node, uint32_t level) = 0;
        virtual void visit(LeafNode *node, uint32_t level) = 0;

    };

    class ChangeTracker {
    public:
        virtual void modified(Node *node, uint32_t level) = 0;
    };

    class NoopTracker : public ChangeTracker {
    public:
        void modified(Node *node, uint32_t level) override {
        }
    };

public:
    BPlusTree() : depth(0), root(allocate_leaf()) {
        assert(N > 2); // N must be greater than two to make the split of two inner nodes sensible.
        assert(M > 0); // Leaf nodes must be able to hold at least one element
    }

    ~BPlusTree() {
    }

public:
    bool empty() const {
        if (depth == 0) {
            return reinterpret_cast<LeafNode *>(root)->num_keys == 0;
        }
        return reinterpret_cast<InnerNode *>(root)->num_keys == 0;
    }

public:
    // Inserts a pair (key, value). If there is a previous pair with
    // the same key, the old value is overwritten with the new one.
    bool add(KEY key, VALUE value) {
        NoopTracker tracker;
        return add(key, value, tracker);
    }

    bool add(KEY key, VALUE value, ChangeTracker &tracker) {
        // GCC warns that this may be used uninitialized, even though that is untrue.
        InsertionResult result = { KEY(), 0, 0, &tracker };
        bool was_split;
        if (depth == 0) {
            // The root is a leaf node
            assert(*reinterpret_cast<NodeType *>(root) == NODE_LEAF);
            was_split = leaf_insert(root->leaf(), key, value, &result);
        } else {
            // The root is an inner node
            assert(*reinterpret_cast<NodeType *>(root) == NODE_INNER);
            was_split = inner_insert(root->inner(), depth, key, value, &result);
        }
        if (was_split) {
            // The old root was splitted in two parts.
            // We have to create a new root pointing to them
            depth++;
            root = allocate_inner();
            InnerNode *rootProxy = root->inner();
            rootProxy->num_keys = 1;
            rootProxy->keys[0] = result.key;
            rootProxy->children[0] = result.left;
            rootProxy->children[1] = result.right;

            tracker.modified(root, depth);
        }

        return true;
    }

    VALUE lookup(const KEY &key) const {
        VALUE value;
        if (find(key, &value)) {
            return value;
        }
        return VALUE{ };
    }

    // Looks for the given key. If it is not found, it returns false,
    // if it is found, it returns true and copies the associated value
    // unless the pointer is null.
    bool find(const KEY &key, VALUE *value = 0) const {
        const InnerNode *inner;
        const Node *node = root;
        unsigned d = depth, index;
        while (d-- != 0) {
            inner = node->inner();
            assert(inner->type == NODE_INNER);
            index = Keys::inner_position_for(key, inner->keys, inner->num_keys);
            assert(index < inner->num_keys + 1);
            node = inner->children[index];
        }
        const LeafNode *leaf = node->leaf();
        assert(leaf->type == NODE_LEAF);
        index = Keys::leaf_position_for(key, leaf->keys, leaf->num_keys);
        assert(index <= leaf->num_keys);
        if (index < leaf->num_keys && leaf->keys[index] == key) {
            if (value != 0) {
                *value = leaf->values[index];
            }
            if (leaf->values[index]) {
                return true;
            }
            else {
                return false;
            }
        } else {
            return false;
        }
    }

    bool find_all(const KEY &key, const KEY &last_key) const {
        const InnerNode *inner;
        const Node *node = root;
        unsigned d = depth, index;
        while (d-- != 0) {
            inner = node->inner();
            assert(inner->type == NODE_INNER);
            index = Keys::inner_position_for(key, inner->keys, inner->num_keys);
            assert(index < inner->num_keys + 1);
            node = inner->children[index];
        }
        const LeafNode *leaf = node->leaf();
        assert(leaf->type == NODE_LEAF);
        index = Keys::leaf_position_for(key, leaf->keys, leaf->num_keys);
        assert(index <= leaf->num_keys);

        auto total_leaves = 0;
        while (leaf != nullptr) {
            total_leaves++;

            auto this_leaf = 0;
            for (unsigned i = 0; i < leaf->num_keys; ++i) {
                if (leaf->keys[i] > key && leaf->keys[i] < last_key) {
                    // btree_key_t this_key = leaf->keys[i];
                    // uint64_t raw = this_key.data;
                    // uint32_t inode = raw & ((uint32_t)-1);
                    // uint32_t offset = (raw >> 32) & ((uint32_t)-1);
                    // std::cout << leaf << " Leaf[" << i << "] = " << inode << " " << offset << " " << total_leaves << std::endl;
                    this_leaf++;
                }
            }

            if (this_leaf == 0) {
                break;
            }

            leaf = leaf->nl;
        }

        return false;
    }

    // Finds the LAST item that is < key. That is, the next item in the tree is not < key, but this
    // item is. If we were to insert key into the tree, it would go after this item. This is weird,
    // but is easier than implementing iterators. In STL terms, this would be "lower_bound(key)--"
    // WARNING: This does *not* work when values are deleted. Thankfully, TPC-C does not use deletes.
    bool find_last_less_then(const KEY &key, VALUE *value = 0, KEY *out_key = 0) const {
        const Node *node = root;
        unsigned int d = depth;
        while (d-- != 0) {
            const InnerNode *inner = node->inner();
            assert(inner->type == NODE_INNER);
            unsigned int pos = Keys::inner_position_for(key, inner->keys, inner->num_keys);
            // We need to rewind in the case where they are equal
            if (pos > 0 && key == inner->keys[pos - 1]) {
                pos -= 1;
            }
            assert(pos == 0 || inner->keys[pos - 1] < key);
            node = inner->children[pos];
        }
        const LeafNode *leaf = node->leaf();
        assert(leaf->type == NODE_LEAF);
        unsigned int pos = Keys::leaf_position_for(key, leaf->keys, leaf->num_keys);
        if (pos <= leaf->num_keys) {
            pos -= 1;
            if (pos < leaf->num_keys && key == leaf->keys[pos]) {
                pos -= 1;
            }

            if (pos < leaf->num_keys) {
                assert(leaf->keys[pos] < key);
                if (leaf->values[pos]) {
                    if (value != NULL) {
                        *value = leaf->values[pos];
                    }
                    if (out_key != NULL) {
                        *out_key = leaf->keys[pos];
                    }
                    return true;
                } else {
                    // This is a deleted key! Try again with the new key value
                    // HACK: This is because this implementation doesn't do deletes correctly. However,
                    // this makes it work. We need this for TPC-C undo. The solution is to use a
                    // more complete b-tree implementation.
                    return find_last_less_then(leaf->keys[pos], value, out_key);
                }
            }
        }

        return false;
    }

    // Looks for the given key. If it is not found, it returns false, if it is
    // found, it returns true and sets the associated value to NULL.
    // Note: Currently leaks memory.
    bool remove(const KEY &key) {
        InnerNode *inner;
        Node *node = root;
        unsigned d = depth, index;
        while (d-- != 0) {
            inner = node->inner();
            assert(inner->type == NODE_INNER);
            index = Keys::inner_position_for(key, inner->keys, inner->num_keys);
            node = inner->children[index];
        }

        LeafNode *leaf = node->leaf();
        assert(leaf->type == NODE_LEAF);
        index = Keys::leaf_position_for(key, leaf->keys, leaf->num_keys);
        if (leaf->keys[index] == key) {
            leaf->values[index] = 0;
            return true;
        } else {
            return false;
        }
    }

    // Returns the size of an inner node
    // It is useful when optimizing performance with cache alignment.
    unsigned sizeof_inner_node() const {
        return sizeof(InnerNode);
    }

    // Returns the size of a leaf node.
    // It is useful when optimizing performance with cache alignment.
    unsigned sizeof_leaf_node() const {
        return sizeof(LeafNode);
    }

    void accept(Node *node, uint32_t level, BPlusTreeVisitor &visitor) {
        if (level > 0) {
            auto inner = node->inner();
            for (size_t i = 0; i <= N; ++i) {
                if (inner->children[i] != nullptr) {
                    accept(inner->children[i], level - 1, visitor);
                }
            }
        }

        if (level > 0) {
            visitor.visit(node->inner(), level);
        }
        else {
            visitor.visit(node->leaf(), level);
        }
    }

    void accept(BPlusTreeVisitor &visitor) {
        accept(root, depth, visitor);
    }

    void dump(std::ostream &os, Node *node, uint32_t level, bool summarize) {
        if (level > 0) {
            os << "Level " << level << " " << node;

            auto inner = node->inner();

            os << " (keys=" << inner->num_keys << ")";

            for (size_t i = 0; i <= N; ++i) {
                if (inner->children[i] != nullptr) {
                    auto key = i != N ? inner->keys[i] : 0;
                    os << " (" << key << "=" << inner->children[i] << ")";
                }
            }

            os << std::endl;

            for (size_t i = 0; i <= N; ++i) {
                if (inner->children[i] != nullptr) {
                    dump(os, inner->children[i], level - 1, summarize);
                }
            }
        }
        else {
            os << "Level " << level << " " << node;

            auto leaf = node->leaf();

            os << " (keys=" << leaf->num_keys << ")";

            for (size_t i = 0; i < leaf->num_keys; ++i) {
                os << " (" << leaf->keys[i] << "=" << leaf->values[i] << ")";
            }

            os << std::endl;
        }
    }

    void dump(std::ostream &os, bool summarize = true) {
        os << "Tree (" << (allocated_leafs + allocated_inners) << " allocations, "
           << allocated_inners << " inner " << allocated_leafs << " leaf):" << std::endl;

        if (!summarize) {
            dump(os, root, depth, summarize);
        }
    }

public:
    // Used when debugging
    enum NodeType {
        NODE_INNER = 0xDEADBEEF,
        NODE_LEAF = 0xC0FFEE
    };

    struct Node {
        InnerNode *inner() {
            return reinterpret_cast<InnerNode*>(this);
        }

        const InnerNode *inner() const {
            return reinterpret_cast<const InnerNode*>(this);
        }

        LeafNode *leaf() {
            return reinterpret_cast<LeafNode*>(this);
        }

        const LeafNode *leaf() const {
            return reinterpret_cast<const LeafNode*>(this);
        }
    };

    // Leaf nodes store pairs of keys and values.
    struct LeafNode : Node {
        #ifndef NDEBUG
        LeafNode() : type(NODE_LEAF), num_keys(0), nl(nullptr) {
            memset(keys, 0, sizeof(keys));
        }
        const NodeType type;
        #else
        LeafNode() : num_keys(0), nl(nullptr) {
            memset(keys, 0, sizeof(keys));
        }
        #endif

        unsigned num_keys;
        KEY keys[M];
        VALUE values[M];
        LeafNode *nl;
    };

    // Inner nodes store pointers to other nodes interleaved with keys.
    struct InnerNode : Node {
        #ifndef NDEBUG
        InnerNode() : type(NODE_INNER), num_keys(0) {
            memset(children, 0, sizeof(children));
        }
        const NodeType type;
        #else
        InnerNode() : num_keys(0) {
            memset(children, 0, sizeof(children));
        }
        #endif

        unsigned num_keys;
        KEY keys[N];
        Node *children[N + 1];
    };

    LeafNode *allocate_leaf() {
        allocated_leafs++;
        return new LeafNode();
    }

    InnerNode *allocate_inner() {
        allocated_inners++;
        return new InnerNode();
    }

    size_t allocated_leafs{ 0 };
    size_t allocated_inners{ 0 };

private:
    // Data type returned by the private insertion methods.
    struct InsertionResult {
        KEY key;
        Node *left;
        Node *right;
        ChangeTracker *tracker;
    };

    bool leaf_insert(LeafNode *node, KEY &key, VALUE &value, InsertionResult *result) {
        assert(node->type == NODE_LEAF);
        assert(node->num_keys <= M);

        auto was_split = false;
        unsigned i = Keys::leaf_position_for(key, node->keys, node->num_keys);
        if (node->num_keys == M) {

            unsigned threshold = (M + 1) / 2;
            auto new_sibling = allocate_leaf();
            new_sibling->num_keys = node->num_keys - threshold;
            for (unsigned j = 0; j < new_sibling->num_keys; ++j) {
                new_sibling->keys[j] = node->keys[threshold + j];
                new_sibling->values[j] = node->values[threshold + j];
            }
            node->num_keys = threshold;
            // NOTE: Jacob added this.
            auto old_nl = node->nl;
            new_sibling->nl = old_nl;
            node->nl = new_sibling;


            if (i < threshold) {
                leaf_insert_nonfull(node, key, value, i);
            }
            else {
                leaf_insert_nonfull(new_sibling, key, value, i - threshold);
            }
            was_split = true;
            result->key = new_sibling->keys[0];
            result->left = node;
            result->right = new_sibling;

            result->tracker->modified(node, 0);
            result->tracker->modified(new_sibling, 0);
        }
        else {
            leaf_insert_nonfull(node, key, value, i);

            result->tracker->modified(node, 0);
        }

        return was_split;
    }

    static void leaf_insert_nonfull(LeafNode *node, KEY &key, VALUE &value, unsigned index) {
        assert(node->type == NODE_LEAF);
        assert(node->num_keys < M);
        assert(index < M);
        assert(index <= node->num_keys);

        if (node->keys[index] == key) {
            // We are inserting a duplicate value. Simply overwrite the old one
            node->values[index] = value;
        }
        else {
            for (auto i = node->num_keys; i > index; --i) {
                node->keys[i] = node->keys[i - 1];
                node->values[i] = node->values[i - 1];
            }
            node->num_keys++;
            node->keys[index] = key;
            node->values[index] = value;
        }
    }

    bool inner_insert(InnerNode *node, unsigned current_depth, KEY &key, VALUE &value, InsertionResult *result) {
        assert(node->type == NODE_INNER);
        assert(current_depth != 0);

        // Early split if node is full.
        // This is not the canonical algorithm for B+ trees,
        // but it is simpler and does not break the definition.
        auto was_split = false;
        if (node->num_keys == N) {
            auto treshold = (N + 1) / 2;
            auto new_sibling = allocate_inner();
            new_sibling->num_keys = node->num_keys - treshold;
            for (unsigned i = 0; i < new_sibling->num_keys; ++i) {
                new_sibling->keys[i] = node->keys[treshold + i];
                new_sibling->children[i] = node->children[treshold + i];
            }
            new_sibling->children[new_sibling->num_keys] = node->children[node->num_keys];
            node->num_keys = treshold - 1;
            // Set up the return variable
            was_split = true;
            result->key = node->keys[treshold - 1];
            result->left = node;
            result->right = new_sibling;
            // Now insert in the appropriate sibling
            if (key < result->key) {
                inner_insert_nonfull(node, current_depth, key, value, result->tracker);
            }
            else {
                inner_insert_nonfull(new_sibling, current_depth, key, value, result->tracker);
            }
        }
        else {
            inner_insert_nonfull(node, current_depth, key, value, result->tracker);
        }
        return was_split;
    }

    void inner_insert_nonfull(InnerNode *node, unsigned current_depth, KEY &key, VALUE &value, ChangeTracker *tracker) {
        assert(node->type == NODE_INNER);
        assert(node->num_keys < N);
        assert(current_depth != 0);

        unsigned index = Keys::inner_position_for(key, node->keys, node->num_keys);
        // GCC warns that this may be used uninitialized, even though that is untrue.
        InsertionResult result = { KEY(), 0, 0, tracker };
        bool was_split;
        if (current_depth - 1 == 0) {
            // The children are leaf nodes
            for (unsigned kk = 0; kk < node->num_keys + 1; ++kk) {
                assert(*reinterpret_cast<NodeType *>(node->children[kk]) == NODE_LEAF);
            }
            was_split = leaf_insert(reinterpret_cast<LeafNode *>(node->children[index]), key, value, &result);
        }
        else {
            // The children are inner nodes
            for (unsigned kk = 0; kk < node->num_keys + 1; ++kk) {
                assert(*reinterpret_cast<NodeType *>(node->children[kk]) == NODE_INNER);
            }
            InnerNode *child = node->children[index]->inner();
            was_split = inner_insert(child, current_depth - 1, key, value, &result);

            if (was_split) {
                tracker->modified(result.left, current_depth - 1);
                tracker->modified(node, current_depth);
            }
        }

        if (was_split) {
            if (index == node->num_keys) {
                // Insertion at the rightmost key
                node->keys[index] = result.key;
                node->children[index] = result.left;
                node->children[index + 1] = result.right;
                node->num_keys++;
            }
            else {
                // Insertion not at the rightmost key
                node->children[node->num_keys + 1] = node->children[node->num_keys];
                for (unsigned i = node->num_keys; i != index; --i) {
                    node->children[i] = node->children[i - 1];
                    node->keys[i] = node->keys[i - 1];
                }
                node->children[index] = result.left;
                node->children[index + 1] = result.right;
                node->keys[index] = result.key;
                node->num_keys++;
            }
        }
        else {
            tracker->modified(node, current_depth);
        }
    }

public:
    //! Depth of the tree. A tree of depth 0 only has a leaf node.
    unsigned depth;
    //! Pointer to the root node. It may be a leaf or an inner node, but it is never null.
    Node *root;
};


}

#endif
