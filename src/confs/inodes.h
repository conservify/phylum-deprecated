#ifndef __CONFS_INODES_H_INCLUDED
#define __CONFS_INODES_H_INCLUDED

#include <confs/backend.h>
#include <confs/block_alloc.h>
#include <confs/persisted_tree.h>

namespace confs {

inline uint64_t make_key(uint32_t upper, uint32_t lower) {
    return ((uint64_t)upper << 32) | (uint64_t)lower;
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
    uint32_t upper() const {
        return (value_ >> 32) & ((uint32_t)-1);
    }

    uint32_t lower() const {
        return (value_) & ((uint32_t)-1);
    }

public:
    friend std::ostream &operator<<(std::ostream &os, const INodeKey &e);

    inline bool operator==(const INodeKey &rhs) const {
        return value_ == rhs.value_;
    }
    inline bool operator!=(const INodeKey &rhs) const {
        return !operator==(rhs);
    }
    inline bool operator<(const INodeKey &rhs) const {
        return value_ < rhs.value_;
    }
    inline bool operator>(const INodeKey &rhs) const {
        return value_ > rhs.value_;
    }
    inline bool operator<=(const INodeKey &rhs) const {
        return !operator>(rhs);
    }
    inline bool operator>=(const INodeKey &rhs) const {
        return !operator<(rhs);
    }
};

inline std::ostream &operator<<(std::ostream &os, const INodeKey &e) {
    return os << "INodeKey<" << e.upper() << " " << e.lower() << ">";
}
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

}

#endif
