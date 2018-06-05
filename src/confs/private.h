#ifndef __CONFS_PRIVATE_H_INCLUDED
#define __CONFS_PRIVATE_H_INCLUDED

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace confs {

enum class confs_status_t {
    Failure,
    Success
};

enum class block_type_t {
    Anchor,
    IndexInner,
    IndexLeaf,
    File,
    INode,
    SuperBlock,
    SuperBlockChain,
    Error,
    Unallocated = 0xff
};

using block_age_t = uint32_t;
using block_index_t = uint32_t;
using page_index_t = uint16_t;
using sector_index_t = uint16_t;
using timestamp_t = uint32_t;

constexpr sector_index_t CONFS_SECTOR_HEAD = 1;
constexpr block_index_t BLOCK_INDEX_INVALID = ((block_index_t)-1);
constexpr sector_index_t SECTOR_INDEX_INVALID = ((sector_index_t)-1);

typedef struct confs_sector_addr_t {
    block_index_t block;
    sector_index_t sector;

    confs_sector_addr_t(block_index_t block = BLOCK_INDEX_INVALID, sector_index_t sector = SECTOR_INDEX_INVALID) :
        block(block), sector(sector) {
    }

    void invalid() {
        block = BLOCK_INDEX_INVALID;
        sector = SECTOR_INDEX_INVALID;
    }

    bool valid() const {
        return block != BLOCK_INDEX_INVALID && sector != SECTOR_INDEX_INVALID;
    }
} confs_sector_addr_t;

typedef struct confs_geometry_t {
    block_index_t number_of_blocks;
    page_index_t pages_per_block;
    sector_index_t sectors_per_page;
    sector_index_t sector_size;

    block_index_t number_of_sectors() const {
        return number_of_blocks * sectors_per_block();
    }

    sector_index_t sectors_per_block() const {
        return pages_per_block * sectors_per_page;
    }

    uint32_t block_size() const {
        return (pages_per_block * sectors_per_page) * sector_size;
    }

    bool valid() const {
        return number_of_blocks > 0 && pages_per_block > 0 && sectors_per_page > 0 && sector_size > 0;
    }

    bool contains(confs_sector_addr_t addr) const {
        return addr.block < number_of_blocks && addr.sector < sectors_per_block();
    }
} confs_geometry_t;

using Geometry = confs_geometry_t;

static const char confs_magic_key[] = "asdfasdf";

typedef struct confs_block_alloc_sector_t {
    block_type_t type;
    block_index_t linked_block;
} confs_block_alloc_sector_t;

typedef struct confs_block_tail_sector_t {

} confs_block_tail_sector_t;

typedef struct confs_block_magic_t {
    char key[sizeof(confs_magic_key)];

    void fill();
    bool valid();
} confs_block_magic_t;

typedef struct confs_block_header_t {
    block_age_t age;
    timestamp_t timestamp;
    confs_block_magic_t magic;
} confs_block_header_t;

typedef struct confs_sb_link_t {
    confs_block_header_t header;
    sector_index_t sector;
    block_index_t chained_block;
} confs_sb_link_t;

typedef struct confs_super_block_t {
    confs_sb_link_t link;
    uint32_t number_of_files;
    block_index_t tree;
} confs_super_block_t;

extern std::ostream &sdebug;

inline std::ostream& operator<<(std::ostream& os, const confs_sector_addr_t &addr) {
    if (!addr.valid()) {
        return os << "<invalid>";
    }
    return os << addr.block << ":" << addr.sector;
}

struct btree_key_t {
    uint64_t data = { 0 };

    btree_key_t(uint64_t data = 0) : data(data) {
    }

    friend std::ostream &operator<<(std::ostream &os, const btree_key_t &e);
};

inline bool operator==(const btree_key_t &lhs, const btree_key_t &rhs) {
    return lhs.data == rhs.data;
}
inline bool operator!=(const btree_key_t &lhs, const btree_key_t &rhs) {
    return !operator==(lhs, rhs);
}
inline bool operator<(const btree_key_t &lhs, const btree_key_t &rhs) {
    return memcmp(&lhs.data, &rhs.data, sizeof(uint64_t)) < 0;
}
inline bool operator>(const btree_key_t &lhs, const btree_key_t &rhs) {
    return operator<(rhs, lhs);
}
inline bool operator<=(const btree_key_t &lhs, const btree_key_t &rhs) {
    return !operator>(lhs, rhs);
}
inline bool operator>=(const btree_key_t &lhs, const btree_key_t &rhs) {
    return !operator<(lhs, rhs);
}

}

#endif
