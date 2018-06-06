#ifndef __CONFS_PRIVATE_H_INCLUDED
#define __CONFS_PRIVATE_H_INCLUDED

#include <iostream>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace confs {

using block_age_t = uint32_t;
using block_index_t = uint32_t;
using page_index_t = uint16_t;
using sector_index_t = uint16_t;
using timestamp_t = uint32_t;

constexpr int32_t SectorSize = 512;

constexpr sector_index_t CONFS_SECTOR_HEAD = 1;
constexpr block_index_t BLOCK_INDEX_INVALID = ((block_index_t)-1);
constexpr sector_index_t SECTOR_INDEX_INVALID = ((sector_index_t)-1);
constexpr uint32_t POSITION_INDEX_INVALID = ((uint32_t)-1);

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

static const char MagicKey[] = "asdfasdf";

struct BlockMagic {
    char key[sizeof(MagicKey)] = { 0 };

    void fill();
    bool valid() const;
};

enum class BlockType {
    Anchor,
    IndexInner,
    IndexLeaf,
    File,
    INode,
    SuperBlock,
    SuperBlockChain,
    Error,
    Unallocated
};

struct BlockAllocSector {
    BlockType type{ BlockType::Error };
    block_index_t linked_block{ 0 };
};

struct BlockTailSector {

};

struct BlockHeader {
    block_age_t age{ 0 };
    timestamp_t timestamp{ 0 };
    BlockMagic magic;
};

struct BlockAddress {
public:
    block_index_t block;
    uint32_t position;

public:
    BlockAddress(block_index_t block = BLOCK_INDEX_INVALID, uint32_t position = POSITION_INDEX_INVALID) :
        block(block), position(position) {
    }

    BlockAddress(SectorAddress addr) : block(addr.block), position(addr.sector * SectorSize) {
    }

public:
    void invalid() {
        block = BLOCK_INDEX_INVALID;
        position = POSITION_INDEX_INVALID;
    }

    bool valid() const {
        return block != BLOCK_INDEX_INVALID && position != POSITION_INDEX_INVALID;
    }

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

extern std::ostream &sdebug;

inline std::ostream& operator<<(std::ostream& os, const SectorAddress &addr) {
    if (!addr.valid()) {
        return os << "<invalid>";
    }
    return os << addr.block << ":" << addr.sector;
}

inline std::ostream& operator<<(std::ostream& os, const Geometry &g) {
    return os << "Geometry<" << g.number_of_blocks << " " << g.pages_per_block << " " << g.sectors_per_page << " " << g.sector_size << ">";
}

inline std::ostream& operator<<(std::ostream& os, const BlockAddress &addr) {
    return os << addr.block << ":" << addr.position;
}

}

#endif
