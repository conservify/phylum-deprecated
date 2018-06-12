#ifndef __PHYLUM_BACKEND_NODES_H_INCLUDED
#define __PHYLUM_BACKEND_NODES_H_INCLUDED

#include "phylum/persisted_tree.h"
#include "phylum/node_serializer.h"

namespace phylum {

struct TreeBlockHeader {
    BlockAllocSector header;

    TreeBlockHeader(BlockType type) : header(type) {
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
    BlockAddress index_;
    BlockAddress leaf_;

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
        if (!storage_->read({ sector, offset }, buffer, required)) {
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
        auto &location = node->depth == 0 ? leaf_ : index_;
        auto type = node->depth == 0 ? BlockType::Leaf : BlockType::Index;
        auto required = serializer.size(head != nullptr);

        // We always dicsard the incoming address. Our memory backend refuses
        // writes to unerased areas.
        if (!location.valid()) {
            location = initialize_block(allocator_->allocate(type), type);
        }
        else {
            // Skip over the previous block.
            location.add(required);

            if (!location.find_room(geometry, required)) {
                location = initialize_block(allocator_->allocate(type), type);
            }
        }

        auto sector = location.sector(geometry);
        auto offset = location.sector_offset(geometry);

        uint8_t buffer[SerializerType::HeadNodeSize];
        if (!serializer.serialize(buffer, node, head)) {
            return { };
        }

        if (!storage_->write({ sector, offset }, buffer, required)) {
            return { };
        }

        return location;
    }

    BlockAddress find_head(block_index_t block) {
        SerializerType serializer;

        assert(block != BLOCK_INDEX_INVALID);

        auto &geometry = storage_->geometry();
        auto required = serializer.size(true);
        auto iter = BlockAddress{ block, 0 };
        auto found = BlockAddress{ };

        // We could compare TreeHead timestamps, though we always append.
        while (iter.remaining_in_block(geometry) > required) {
            TreeHead head;
            NodeType node;

            if (iter.beginning_of_block()) {
                TreeBlockHeader header(BlockType::Error);
                if (!storage_->read(iter, &header, sizeof(TreeBlockHeader))) {
                    return { };
                }

                if (!header.valid()) {
                    return found;
                }

                iter.add(SectorSize);
            }
            else {
                if (deserialize(iter, &node, &head)) {
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

private:
    BlockAddress initialize_block(block_index_t block, BlockType type) {
        TreeBlockHeader header(type);

        header.fill();

        if (!storage_->erase(block)) {
            return { };
        }

        if (!storage_->write({ block, 0 }, &header, sizeof(TreeBlockHeader))) {
            return { };
        }

        return BlockAddress { block, SectorSize };
    }

};

}

#endif
