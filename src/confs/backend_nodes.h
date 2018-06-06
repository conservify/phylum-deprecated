#ifndef __CONFS_BACKEND_NODES_H_INCLUDED
#define __CONFS_BACKEND_NODES_H_INCLUDED

#include "confs/persisted_tree.h"
#include "confs/node_serializer.h"

namespace confs {

struct TreeBlockHeader {
    BlockAllocSector header;

    TreeBlockHeader() : header(BlockType::Tree) {
    }

    void fill() {
        header.magic.fill();
        header.age = 0;
        header.timestamp = 0;
    }

    bool valid() const {
        return header.valid();
    }
};

inline std::ostream& operator<<(std::ostream& os, const TreeBlockHeader &h) {
    return os << "TreeBlock<" << h.header << ">";
}

template<typename NODE>
class StorageBackendNodeStorage : public NodeStorage<NODE, BlockAddress> {
public:
    using NodeType = NODE;
    using SerializerType = NodeSerializer<NodeType>;

private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    BlockAddress location_;

public:
    StorageBackendNodeStorage(StorageBackend &storage, BlockAllocator &allocator) : storage_(&storage), allocator_(&allocator) {
    }

public:
    bool deserialize(BlockAddress addr, NodeType *node, TreeHead *head) {
        SerializerType serializer;

        auto &geometry = storage_->geometry();

        auto sector = addr.sector(geometry);
        auto offset = addr.sector_offset(geometry);
        auto required = serializer.size(head != nullptr);

        uint8_t buffer[SerializerType::HeadNodeSize];
        if (!storage_->read(sector, offset, buffer, required)) {
            return false;
        }

        if (!serializer.deserialize(buffer, node, head)) {
            return false;
        }

        return true;
    }

    BlockAddress serialize(BlockAddress addr, const NodeType *node, const TreeHead *head) {
        SerializerType serializer;

        auto &geometry = storage_->geometry();

        auto required = serializer.size(head != nullptr);

        // We always dicsard the incoming address. Our memory backend refuses
        // writes to unerased areas.
        if (!location_.valid()) {
            location_ = initialize_block(allocator_->allocate());
        }
        else {
            location_.add(required);

            if (!location_.find_room(geometry, required)) {
                location_ = initialize_block(allocator_->allocate());
            }
        }

        auto sector = location_.sector(geometry);
        auto offset = location_.sector_offset(geometry);

        uint8_t buffer[SerializerType::HeadNodeSize];
        if (!serializer.serialize(buffer, node, head)) {
            return { };
        }

        if (!storage_->write(sector, offset, buffer, required)) {
            return { };
        }

        return location_;
    }

private:
    BlockAddress initialize_block(block_index_t block) {
        TreeBlockHeader header;

        header.fill();

        if (!storage_->erase(block)) {
            return { };
        }

        if (!storage_->write({ block, 0 }, 0, &header, sizeof(TreeBlockHeader))) {
            return { };
        }

        return BlockAddress { block, SectorSize };
    }

};

}

#endif
