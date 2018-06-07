#ifndef __CONFS_INODES_H_INCLUDED
#define __CONFS_INODES_H_INCLUDED

#include <confs/backend.h>
#include <confs/block_alloc.h>
#include <confs/persisted_tree.h>

namespace confs {

class INodeKey {
private:
    uint64_t value_;

public:
    INodeKey(uint64_t value = 0) : value_(value) {
    }

    INodeKey(uint32_t upper, uint32_t lower) : value_(make(upper, lower)) {
    }

public:
    uint32_t upper() const {
        return (value_ >> 32) & ((uint32_t)-1);
    }

    uint32_t lower() const {
        return (value_) & ((uint32_t)-1);
    }

    operator uint64_t() const {
        return value_;
    }

    static uint64_t make(uint32_t upper, uint32_t lower) {
        return ((uint64_t)upper << 32) | (uint64_t)lower;
    }

};

inline std::ostream &operator<<(std::ostream &os, const INodeKey &e) {
    return os << "INodeKey<" << e.upper() << " " << e.lower() << ">";
}

}

#endif
