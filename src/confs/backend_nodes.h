#ifndef __CONFS_BACKEND_NODES_H_INCLUDED
#define __CONFS_BACKEND_NODES_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/node_serializer.h"

namespace confs {

template<typename NODE>
class StorageBackendNodeStorage : public NodeStorage<NODE, SectorAddress> {
public:
    using NodeType = NODE;
    using SerializerType = NodeSerializer<NodeType>;

private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    SectorAddress location_;

public:
    StorageBackendNodeStorage(StorageBackend &storage, BlockAllocator &allocator) : storage_(&storage), allocator_(&allocator) {
    }

public:
    bool deserialize(SectorAddress addr, NodeType *node, TreeHead *head) {
        SerializerType serializer;

        uint8_t buffer[512];
        if (!storage_->read(addr, buffer, serializer.size(node, head))) {
            return false;
        }

        return serializer.deserialize(buffer, node, head);
    }

    SectorAddress serialize(SectorAddress addr, const NodeType *node, const TreeHead *head) {
        SerializerType serializer;

        // We always dicsard the incoming address. Our memory backend refuses
        // writes to unerased areas.
        if (!location_.valid()) {
            auto block = allocator_->allocate();
            if (!storage_->erase(block)) {
                return { };
            }

            location_ = { block, 0 };
        }
        else {
            location_.sector++;
            if (location_.sector == storage_->geometry().sectors_per_block()) {
                location_ = allocator_->allocate();
            }
        }

        uint8_t buffer[512];
        if (!serializer.serialize(buffer, node, head)) {
            return { };
        }

        if (!storage_->write(location_, buffer, serializer.size(node, head))) {
            return { };
        }

        return location_;
    }

};

}

#endif
