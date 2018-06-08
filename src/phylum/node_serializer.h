#ifndef __PHYLUM_NODES_SERIALIZER_H_INCLUDED
#define __PHYLUM_NODES_SERIALIZER_H_INCLUDED

namespace phylum {

template<typename NODE>
class NodeSerializer {
public:
    using NodeType = NODE;
    using KEY = typename NODE::KeyType;
    using VALUE = typename NODE::ValueType;
    using ADDRESS = typename NODE::AddressType;

private:
    struct serialized_inner_node_t {
        DepthType level;
        uint16_t size;
        IndexType number_keys;
        KEY keys[NODE::InnerSize];
        ADDRESS children[NODE::InnerSize + 1];
    };

    struct serialized_leaf_node_t {
        DepthType level;
        uint16_t size;
        IndexType number_keys;
        KEY keys[NODE::LeafSize];
        VALUE values[NODE::LeafSize];
    };

    union serialized_node_t {
        DepthType level;
        serialized_inner_node_t inner;
        serialized_leaf_node_t leaf;
    };

    struct serialized_head_t {
        BlockMagic magic;
        timestamp_t timestamp;
        serialized_node_t node;
    };

public:
    static constexpr size_t HeadNodeSize = sizeof(serialized_head_t);
    static constexpr size_t NodeSize = sizeof(serialized_node_t);

public:
    bool deserialize(const void *ptr, NodeType *node, TreeHead *head) {
        if (head) {
            return deserialize(reinterpret_cast<const serialized_head_t*>(ptr), node);
        }
        return deserialize(reinterpret_cast<const serialized_node_t*>(ptr), node);
    }

    bool serialize(void *ptr, const NodeType *node, const TreeHead *head) {
        if (head) {
            return serialize(node, reinterpret_cast<serialized_head_t*>(ptr));
        }
        return serialize(node, reinterpret_cast<serialized_node_t*>(ptr));
    }

    size_t size(bool head) {
        // Pretend all nodes are the same size, in this case the size of the
        // head node. This makes some things easier and with how big things are
        // now it doesn't really make a difference. 128 vs 112 doesn't give us
        // any extra room unless we split nodes across sectors.
        return sizeof(serialized_head_t);
    }

private:
    bool serialize(const NodeType *node, serialized_head_t *s) {
        s->magic.fill();

        return serialize(node, &s->node);
    }

    bool deserialize(const serialized_head_t *s, NodeType *node) {
        if (!s->magic.valid()) {
            return false;
        }
        return deserialize(&s->node, node);
    }

    bool serialize(const NodeType *node, serialized_node_t *s) {
        if (node->depth == 0) {
            s->leaf.level = node->depth;
            s->leaf.size = sizeof(serialized_node_t);
            s->leaf.number_keys = node->number_keys;
            memcpy(&s->leaf.keys, &node->keys, sizeof(node->keys));
            memcpy(&s->leaf.values, &node->d.values, sizeof(node->d.values));
        }
        else {
            assert(!node->empty());
            s->inner.level = node->depth;
            s->inner.size = sizeof(serialized_node_t);
            s->inner.number_keys = node->number_keys;
            memcpy(&s->inner.keys, &node->keys, sizeof(node->keys));
            for (auto i = (IndexType)0; i < NODE::InnerSize + 1; ++i) {
                if (i <= node->number_keys) {
                    assert(node->d.children[i].address().valid());
                }
                s->inner.children[i] = node->d.children[i].address();
            }
        }
        return true;
    }

    bool deserialize(const serialized_node_t *s, NodeType *node) {
        assert(sizeof(node->keys) == sizeof(s->leaf.keys));
        assert(sizeof(node->d.values) == sizeof(s->leaf.values));

        if (s->level == 0) {
            node->depth = s->leaf.level;
            node->number_keys = s->leaf.number_keys;
            memcpy(&node->keys, &s->leaf.keys, sizeof(node->keys));
            memcpy(&node->d.values, &s->leaf.values, sizeof(node->d.values));
        }
        else {
            node->depth = s->inner.level;
            node->number_keys = s->inner.number_keys;
            memcpy(&node->keys, &s->inner.keys, sizeof(node->keys));
            for (auto i = (IndexType)0; i < NODE::InnerSize + 1; ++i) {
                node->d.children[i].clear();
                if (s->inner.children[i].valid()) {
                    node->d.children[i].address(s->inner.children[i]);
                }
            }
            assert(!node->empty());
        }
        return true;
    }

};

}

#endif
