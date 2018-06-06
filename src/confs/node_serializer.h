#ifndef __CONFS_NODES_SERIALIZER_H_INCLUDED
#define __CONFS_SENODES_RIALIZER_H_INCLUDED

namespace confs {

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

    size_t size(const NodeType *node, bool head) {
        if (head) {
            return sizeof(serialized_head_t);
        }
        return sizeof(serialized_node_t);
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
            memcpy(&s->leaf.values, &node->values, sizeof(node->values));
        }
        else {
            assert(!node->empty());
            s->inner.level = node->depth;
            s->inner.size = sizeof(serialized_node_t);
            s->inner.number_keys = node->number_keys;
            memcpy(&s->inner.keys, &node->keys, sizeof(node->keys));
            for (auto i = (IndexType)0; i < NODE::InnerSize + 1; ++i) {
                if (i <= node->number_keys) {
                    assert(node->children[i].address().valid());
                }
                s->inner.children[i] = node->children[i].address();
            }
        }
        return true;
    }

    bool deserialize(const serialized_node_t *s, NodeType *node) {
        assert(sizeof(node->keys) == sizeof(s->leaf.keys));
        assert(sizeof(node->values) == sizeof(s->leaf.values));

        if (s->level == 0) {
            node->depth = s->leaf.level;
            node->number_keys = s->leaf.number_keys;
            memcpy(&node->keys, &s->leaf.keys, sizeof(node->keys));
            memcpy(&node->values, &s->leaf.values, sizeof(node->values));
        }
        else {
            node->depth = s->inner.level;
            node->number_keys = s->inner.number_keys;
            memcpy(&node->keys, &s->inner.keys, sizeof(node->keys));
            for (auto i = (IndexType)0; i < NODE::InnerSize + 1; ++i) {
                node->children[i].clear();
                if (s->inner.children[i].valid()) {
                    node->children[i].address(s->inner.children[i]);
                }
            }
            assert(!node->empty());
        }
        return true;
    }

};

}

#endif
