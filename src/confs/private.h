#ifndef __CONFS_PRIVATE_H_INCLUDED
#define __CONFS_PRIVATE_H_INCLUDED

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace confs {

enum class BlockType {
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

/**
 *
 */
constexpr int32_t SectorSize = 512;

struct SectorAddress {
    block_index_t block;
    sector_index_t sector;

    SectorAddress(block_index_t block = BLOCK_INDEX_INVALID, sector_index_t sector = SECTOR_INDEX_INVALID) :
        block(block), sector(sector) {
    }

    void invalid() {
        block = BLOCK_INDEX_INVALID;
        sector = SECTOR_INDEX_INVALID;
    }

    bool valid() const {
        return block != BLOCK_INDEX_INVALID && sector != SECTOR_INDEX_INVALID;
    }
};

struct Geometry {
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
        if (sector_size != SectorSize) {
            return false;
        }
        return number_of_blocks > 0 && pages_per_block > 0 && sectors_per_page > 0 && sector_size > 0;
    }

    bool contains(SectorAddress addr) const {
        return addr.block < number_of_blocks && addr.sector < sectors_per_block();
    }
};

inline std::ostream& operator<<(std::ostream& os, const Geometry &g) {
    return os << "Geometry<" << g.number_of_blocks << " " << g.pages_per_block << " " << g.sectors_per_page << " " << g.sector_size << ">";
}

static const char MagicKey[] = "asdfasdf";

struct BlockMagic {
    char key[sizeof(MagicKey)];

    void fill();
    bool valid() const;
};

struct BlockAllocSector {
    BlockType type;
    block_index_t linked_block;
};

struct BlockTailSector {

};

struct BlockHeader {
    block_age_t age;
    timestamp_t timestamp;
    BlockMagic magic;
};

extern std::ostream &sdebug;

inline std::ostream& operator<<(std::ostream& os, const SectorAddress &addr) {
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

struct BlockAddress {
    block_index_t block;
    uint32_t position;

    bool valid() const {
        return block != 0;
    }

    void invalid() {
        block = 0;
    }
};

inline std::ostream& operator<<(std::ostream& os, const BlockAddress &addr) {
    return os << addr.block << ":" << addr.position;
}

struct BlockIterator {
public:
    block_index_t block;
    uint32_t position;

public:
    BlockIterator() {
    }

    BlockIterator(BlockAddress addr) : block(addr.block), position(addr.position) {
    }

    BlockIterator(block_index_t block, uint32_t position = 0) : block(block), position(position) {
    }

    BlockIterator(SectorAddress addr) : block(addr.block), position(addr.sector * SectorSize) {
    }

public:
    uint32_t remaining_in_sector(Geometry &g) {
        return g.sector_size - (position % g.sector_size);
    }

    uint32_t remaining_in_block(Geometry &g) {
        return g.block_size() - position;
    }

    sector_index_t sector_offset(Geometry &g) {
        return position % g.sector_size;
    }

    sector_index_t sector_number(Geometry &g) {
        return position / g.sector_size;
    }

    SectorAddress sector(Geometry &g) {
        return { block, sector_number(g) };
    }

    void seek(uint32_t n) {
        position = n;
    }

    void add(uint32_t n) {
        position += n;
    }

    bool find_room(Geometry &g, uint32_t n) {
        assert(n <= g.sector_size);

        auto sector_remaining = remaining_in_sector(g);
        if (sector_remaining > n) {
            return true;
        }

        auto block_remaining = remaining_in_block(g);
        if (block_remaining < n) {
            return false;
        }

        position += sector_remaining;

        return true;
    }

    BlockAddress address() {
        return { block, position };
    }

};

inline std::ostream& operator<<(std::ostream& os, const BlockIterator &iter) {
    return os << iter.block << ":" << iter.position;
}

}

#endif
