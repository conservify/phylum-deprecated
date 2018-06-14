#ifndef __PHYLUM_INODES_H_INCLUDED
#define __PHYLUM_INODES_H_INCLUDED

#include <phylum/backend.h>
#include <phylum/block_alloc.h>
#include <phylum/persisted_tree.h>
#include <phylum/crc.h>

namespace phylum {

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

public:
    static uint32_t file_id(const char *name) {
        return crc32_checksum((uint8_t *)name, strlen(name));
    }

    static INodeKey file_beginning(uint32_t id) {
        return make(id, 0);
    }

    static INodeKey file_position(uint32_t id, uint32_t length) {
        return make(id, length);
    }

    static INodeKey file_beginning(const char *name) {
        return file_position(file_id(name), 0);
    }

    static INodeKey file_maximum(const char *name) {
        return make(file_id(name), ((uint32_t)-1));
    }

    static INodeKey file_maximum(uint32_t id) {
        return make(id, ((uint32_t)-1));
    }

};

inline std::ostream &operator<<(std::ostream &os, const INodeKey &e) {
    return os << "INodeKey<" << e.upper() << " " << e.lower() << ">";
}

}

#endif
