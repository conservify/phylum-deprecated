#ifndef __PHYLUM_PRIVATE_H_INCLUDED
#define __PHYLUM_PRIVATE_H_INCLUDED

#ifndef ARDUINO
#include <iostream>
#endif

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "phylum/platform.h"

namespace phylum {

using block_age_t = uint32_t;
using block_index_t = uint32_t;
using page_index_t = uint16_t;
using sector_index_t = uint16_t;
using timestamp_t = uint32_t;
using file_id_t = uint32_t;

constexpr int32_t SectorSize = 512;

constexpr sector_index_t SECTOR_HEAD = 1;
constexpr timestamp_t TIMESTAMP_INVALID = ((timestamp_t)-1);
constexpr block_index_t BLOCK_INDEX_INVALID = ((block_index_t)-1);
constexpr sector_index_t SECTOR_INDEX_INVALID = ((sector_index_t)-1);
constexpr block_age_t BLOCK_AGE_INVALID = ((block_age_t)-1);
constexpr uint32_t POSITION_INDEX_INVALID = ((uint32_t)-1);
constexpr file_id_t FILE_ID_INVALID = ((file_id_t)-1);

struct BlockMagic {
    static constexpr char MagicKey[] = "phylummm";

    char key[sizeof(MagicKey)] = { 0 };

    void fill();
    bool valid() const;
};

enum class BlockType {
    Anchor,
    SuperBlockLink,
    SuperBlock,
    Journal,
    File,
    Tree,
    Error,
    Unallocated
};

struct BlockAllocSector {
    BlockMagic magic;
    BlockType type;
    block_age_t age{ BLOCK_AGE_INVALID };
    timestamp_t timestamp{ TIMESTAMP_INVALID };
    block_index_t linked_block{ BLOCK_INDEX_INVALID };

    BlockAllocSector(BlockType type = BlockType::Error) : type(type) {
    }

    void fill() {
        magic.fill();
    }

    bool valid() const {
        return magic.valid();
    }
};

struct SectorTail {
    uint16_t bytes;
};

struct BlockTail {
    SectorTail sector;
    uint32_t bytes_in_block{ 0 };
    block_index_t linked_block{ BLOCK_INDEX_INVALID };

    BlockTail() {
    }
};

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

struct BlockAddress;

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

    bool contains(const SectorAddress addr) const {
        return addr.block < number_of_blocks && addr.sector < sectors_per_block();
    }

    bool contains(const BlockAddress addr) const;
};

struct BlockAddress {
public:
    block_index_t block;
    uint32_t position;

public:
    BlockAddress(block_index_t block = BLOCK_INDEX_INVALID, uint32_t position = POSITION_INDEX_INVALID) :
        block(block), position(position) {
    }

    BlockAddress(SectorAddress addr, uint32_t offset = 0) : block(addr.block), position(addr.sector * SectorSize + offset) {
    }

public:
    void invalid() {
        block = BLOCK_INDEX_INVALID;
        position = POSITION_INDEX_INVALID;
    }

    bool valid() const {
        return block != BLOCK_INDEX_INVALID && position != POSITION_INDEX_INVALID;
    }

    uint32_t remaining_in_sector(const Geometry &g) {
        return g.sector_size - (position % g.sector_size);
    }

    uint32_t remaining_in_block(const Geometry &g) {
        return g.block_size() - position;
    }

    sector_index_t sector_offset(const Geometry &g) {
        return position % g.sector_size;
    }

    sector_index_t sector_number(const Geometry &g) {
        return position / g.sector_size;
    }

    SectorAddress sector(const Geometry &g) {
        return { block, sector_number(g) };
    }

    BlockAddress address() {
        return { block, position };
    }

    void seek(uint32_t n) {
        position = n;
    }

    void add(uint32_t n) {
        position += n;
    }

    bool beginning_of_block() {
        return position == 0;
    }

    bool find_room(const Geometry &g, uint32_t n) {
        assert(n <= g.sector_size);

        auto block_remaining = remaining_in_block(g);
        if (n > block_remaining) {
            return false;
        }

        auto sector_remaining = remaining_in_sector(g);
        if (sector_remaining >= n) {
            return true;
        }

        position += sector_remaining;

        return true;
    }

    bool tail_sector(const Geometry &g) const {
        return position >= g.block_size() - g.sector_size;
    }

    static BlockAddress tail_sector_of(block_index_t block, const Geometry &g) {
        return { block, g.block_size() - g.sector_size };
    }

    static BlockAddress from(uint64_t value) {
        uint32_t block = value >> 32;
        uint32_t position = value & ((uint32_t)-1);
        return BlockAddress{ block, position };
    }

    operator uint64_t() const {
        return (((uint64_t)block) << 32) | position;
    }

};

inline bool Geometry::contains(const BlockAddress addr) const {
    return addr.block < number_of_blocks && addr.position < block_size();
}

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
    auto sector = addr.position / SectorSize;
    auto offset = addr.position % SectorSize;
    return os << addr.block << ":" << sector << "+" << offset;
}

inline std::ostream& operator<<(std::ostream& os, const BlockType &t) {
    switch (t) {
    case BlockType::Anchor: return os << "Anchor";
    case BlockType::SuperBlockLink: return os << "SuperBlockLink";
    case BlockType::SuperBlock: return os << "SuperBlock";
    case BlockType::File: return os << "File";
    case BlockType::Tree: return os << "Tree";
    case BlockType::Error: return os << "Error";
    default: {
        return os << "<unknown>";
    }
    }
}

inline std::ostream& operator<<(std::ostream& os, const BlockAllocSector &h) {
    return os << "BAS<type=" << h.type << " age=" << h.age << " ts=" << h.timestamp << " link=" << h.linked_block << ">";
}

inline std::ostream& operator<<(std::ostream& os, const BlockTail &h) {
    return os << "BlockTail<>";
}

}

#endif
