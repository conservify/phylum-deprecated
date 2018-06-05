#ifndef __CONFS_INODES_H_INCLUDED
#define __CONFS_INODES_H_INCLUDED

#include <confs/backend.h>
#include <confs/block_alloc.h>
#include <confs/persisted_tree.h>

namespace confs {

inline uint64_t make_key(uint32_t inode, uint32_t offset) {
    return ((uint64_t)offset << 32) | (uint64_t)inode;
}

class INodeKey {
private:
    uint64_t value_;

public:
    INodeKey(uint64_t value = 0) : value_(value) {
    }

    INodeKey(uint32_t upper, uint32_t lower) : value_(make_key(upper, lower)) {
    }

public:

public:
    friend std::ostream &operator<<(std::ostream &os, const INodeKey &e);

    inline bool operator==(const INodeKey &rhs) const {
        return value_ == rhs.value_;
    }
    inline bool operator!=(const INodeKey &rhs) const {
        return !operator==(rhs);
    }
    inline bool operator<(const INodeKey &rhs) const {
        return memcmp(&value_, &rhs.value_, sizeof(uint64_t)) < 0;
    }
    inline bool operator>(const INodeKey &rhs) const {
        return memcmp(&value_, &rhs.value_, sizeof(uint64_t)) > 0;
    }
    inline bool operator<=(const INodeKey &rhs) const {
        return !operator>(rhs);
    }
    inline bool operator>=(const INodeKey &rhs) const {
        return !operator<(rhs);
    }
};

/*
  inline bool operator==(const INodeKey &lhs, const INodeKey &rhs) {
  return lhs.value_ == rhs.value_;
  }
  inline bool operator!=(const INodeKey &lhs, const INodeKey &rhs) {
  return !operator==(lhs, rhs);
  }
  inline bool operator<(const INodeKey &lhs, const INodeKey &rhs) {
  return memcmp(&lhs.value_, &rhs.value_, sizeof(uint64_t)) < 0;
  }
  inline bool operator>(const INodeKey &lhs, const INodeKey &rhs) {
  return operator<(rhs, lhs);
  }
  inline bool operator<=(const INodeKey &lhs, const INodeKey &rhs) {
  return !operator>(lhs, rhs);
  }
  inline bool operator>=(const INodeKey &lhs, const INodeKey &rhs) {
  return !operator<(lhs, rhs);
  }
*/

template<typename NODE, typename ADDRESS = typename NODE::AddressType>
class StorageBackendNodeStorage : public NodeStorage<NODE, ADDRESS> {
public:
    using NodeType = NODE;
    using SerializerType = NodeSerializer<NodeType>;

private:
    StorageBackend *storage_;
    BlockAllocator *allocator_;
    confs_sector_addr_t location_;

public:
    StorageBackendNodeStorage(StorageBackend &storage, BlockAllocator &allocator) : storage_(&storage), allocator_(&allocator) {
    }

public:
    void deserialize(ADDRESS addr, NodeType *node) {
        SerializerType serializer;

        uint8_t buffer[512];
        if (!storage_->read(addr, buffer, serializer.size())) {
            return;
        }

        serializer.deserialize(buffer, node);
    }

    ADDRESS serialize(ADDRESS addr, NodeType *node) {
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

        // sdebug << "Serialize: " << location_ << std::endl;

        uint8_t buffer[512];
        serializer.serialize(buffer, node);

        if (!storage_->write(location_, buffer, serializer.size())) {
            return { };
        }

        return location_;
    }

};

}

#endif
