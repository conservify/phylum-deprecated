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
constexpr uint32_t INVALID_SEQUENCE_NUMBER = ((uint32_t)-1);

struct BlockMagic {
    static constexpr char MagicKey[] = "phylum00";

    char key[sizeof(MagicKey)] = { 0 };

    BlockMagic() {
    }

    BlockMagic(const char *k) {
        assert(k == MagicKey); // :)
        memcpy(key, k, sizeof(MagicKey));
    }

    static BlockMagic get_valid() {
        return { MagicKey };
    }

    void fill();
    bool valid() const;
};

enum class BlockType : uint8_t {
    Zero,
    Anchor,
    SuperBlockLink,
    SuperBlock,
    Journal,
    File,
    Leaf,
    Index,
    Free,
    Error,
    Unallocated
};

struct BlockHead {
    BlockMagic magic;
    BlockType type;
    block_age_t age{ BLOCK_AGE_INVALID };
    timestamp_t timestamp{ TIMESTAMP_INVALID };
    block_index_t linked_block{ BLOCK_INDEX_INVALID };

    BlockHead(BlockType type = BlockType::Error) : type(type) {
    }

    void fill() {
        magic.fill();
    }

    bool valid() const {
        return magic.valid();
    }
};

struct BlockTail {
    block_index_t linked_block{ BLOCK_INDEX_INVALID };
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

    bool operator==(const SectorAddress &other) const {
        return block == other.block && sector == other.sector;
    }

    bool operator!=(const SectorAddress &other) const {
        return !(*this == other);
    }
};

struct BlockAddress;

struct Geometry {
    block_index_t first_{ 0 };
    block_index_t number_of_blocks{ 0 };
    page_index_t pages_per_block{ 0 };
    sector_index_t sectors_per_page{ 0 };
    sector_index_t sector_size{ 0 };

    Geometry() {
    }

    Geometry(block_index_t number_of_blocks, page_index_t pages_per_block, sector_index_t sectors_per_page, sector_index_t sector_size) :
        number_of_blocks(number_of_blocks), pages_per_block(pages_per_block), sectors_per_page(sectors_per_page), sector_size(sector_size) {
    }

    block_index_t first() const {
        return first_;
    }

    block_index_t number_of_sectors() const {
        return number_of_blocks * sectors_per_block();
    }

    sector_index_t sectors_per_block() const {
        return pages_per_block * sectors_per_page;
    }

    Geometry file_geometry() const {
        auto original_block_size = block_size();
        return {
            number_of_blocks,
            (page_index_t)(original_block_size / SectorSize / sectors_per_page),
            sectors_per_page,
            SectorSize
        };
    }

    uint32_t block_size() const {
        return (pages_per_block * sectors_per_page) * sector_size;
    }

    uint64_t size() const {
        return (uint64_t)block_size() * (uint64_t)number_of_blocks;
    }

    bool valid() const {
        return number_of_blocks > 0 && pages_per_block > 0 && sectors_per_page > 0 && sector_size > 0;
    }

    bool contains(const SectorAddress addr) const {
        return addr.block < number_of_blocks && addr.sector < sectors_per_block();
    }

    bool contains(const BlockAddress addr) const;

    bool valid(const BlockAddress addr) const;

    BlockAddress block_tail_address_from(BlockAddress addr, size_t sz) const;

    uint32_t remaining_in_block(BlockAddress addr, size_t tail_size) const;

    static Geometry from_physical_block_layout(uint32_t number_of_physical_blocks, sector_index_t sector_size) {
        return from_physical_block_layout(Geometry{ 0, 4, 4, sector_size }, number_of_physical_blocks);
    }

    static Geometry from_physical_block_layout(Geometry g, uint32_t number_of_physical_blocks) {
        auto number_of_fs_blocks = number_of_physical_blocks / (g.sectors_per_page * g.pages_per_block);
        g.number_of_blocks = number_of_fs_blocks;
        return g;
    }
};

struct BlockAddress {
public:
    block_index_t block;
    uint32_t position;

public:
    BlockAddress() : block(BLOCK_INDEX_INVALID), position(POSITION_INDEX_INVALID) {
    }

    BlockAddress(block_index_t block, uint32_t position) : block(block), position(position) {
    }

    BlockAddress(const Geometry &geometry, SectorAddress addr, uint32_t offset) : block(addr.block), position(addr.sector * geometry.sector_size + offset) {
    }

public:
    void invalid() {
        block = BLOCK_INDEX_INVALID;
        position = POSITION_INDEX_INVALID;
    }

    bool valid() const {
        return block != BLOCK_INDEX_INVALID && position != POSITION_INDEX_INVALID;
    }

    bool zero() const {
        return block == 0 && position == 0;
    }

    uint32_t remaining_in_sector(const Geometry &g) {
        return g.sector_size - (position % g.sector_size);
    }

    uint32_t remaining_in_block(const Geometry &g) {
        return g.block_size() - position;
    }

    sector_index_t sector_offset(const Geometry &g) {
        return sector_offset(g.sector_size);
    }

    sector_index_t sector_offset(sector_index_t sector_size) {
        return position % sector_size;
    }

    sector_index_t sector_number(const Geometry &g) {
        return position / g.sector_size;
    }

    SectorAddress sector(const Geometry &g) {
        return { block, sector_number(g) };
    }

    void seek(uint32_t n) {
        position = n;
    }

    void add(uint32_t n) {
        position += n;
    }

    bool is_beginning_of_block() const {
        return position == 0;
    }

    BlockAddress beginning_of_block() const {
        return { block, 0 };
    }

    BlockAddress advance(size_t s) const {
        return { block, position + (uint32_t)s };
    }

    bool add_or_move_to_following_sector(const Geometry &g, uint32_t n) {
        assert(n <= g.sector_size);

        auto block_remaining = remaining_in_block(g);
        if (n > block_remaining) {
            return false;
        }

        if (remaining_in_sector(g) >= n) {
            position += n;
        }

        auto sector_remaining = remaining_in_sector(g);
        if (sector_remaining >= n) {
            return true;
        }

        position += sector_remaining;

        return true;
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

    bool can_write_entry_before_tail(const Geometry &g, uint32_t entry_size, uint32_t tail_size) {
        assert(entry_size + tail_size <= g.sector_size);

        auto block_remaining = remaining_in_block(g);
        if (block_remaining >= entry_size + tail_size) {
            return true;
        }

        return false;
    }

    bool tail_sector(const Geometry &g) const {
        return position >= g.block_size() - g.sector_size;
    }

    static BlockAddress tail_data_of(block_index_t block, const Geometry &g, size_t size) {
        return BlockAddress{ block, g.block_size() - (uint32_t)size };
    }

    static BlockAddress tail_sector_of(block_index_t block, const Geometry &g) {
        return { block, g.block_size() - g.sector_size };
    }

    static BlockAddress from(uint64_t value) {
        uint32_t block = value >> 32;
        uint32_t position = value & ((uint32_t)-1);
        return BlockAddress{ block, position };
    }

    uint64_t value() {
        return (((uint64_t)block) << 32) | position;
    }

    bool operator==(const BlockAddress &other) const {
        return block == other.block && position == other.position;
    }

    bool operator!=(const BlockAddress &other) const {
        return !(*this == other);
    }

};

inline bool is_valid_block(block_index_t block) {
    return block != BLOCK_INDEX_INVALID && block != 0;
}

inline bool Geometry::contains(const BlockAddress addr) const {
    return addr.block < number_of_blocks && addr.position < block_size();
}

inline bool Geometry::valid(const BlockAddress addr) const {
    return contains(addr);
}

inline BlockAddress Geometry::block_tail_address_from(BlockAddress addr, size_t sz) const {
    return BlockAddress{ addr.block, block_size() - (uint32_t)sz };
}

inline uint32_t Geometry::remaining_in_block(BlockAddress addr, size_t tail_size) const {
    return block_size() - (addr.position + tail_size);
}

inline ostreamtype& operator<<(ostreamtype& os, const SectorAddress &addr) {
    if (!addr.valid()) {
        return os << "<invalid>";
    }
    return os << addr.block << ":" << addr.sector;
}

inline ostreamtype& operator<<(ostreamtype& os, const Geometry &g) {
    return os << "Geometry<" << g.number_of_blocks << " " << g.pages_per_block << " " << g.sectors_per_page << " " << g.sector_size << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const BlockAddress &addr) {
    return os << addr.block << ":" << addr.position;
}

inline ostreamtype& operator<<(ostreamtype& os, const BlockType &t) {
    switch (t) {
    case BlockType::Anchor: return os << "Anchor";
    case BlockType::SuperBlockLink: return os << "SuperBlockLink";
    case BlockType::SuperBlock: return os << "SuperBlock";
    case BlockType::Journal: return os << "Journal";
    case BlockType::File: return os << "File";
    case BlockType::Leaf: return os << "Leaf";
    case BlockType::Index: return os << "Index";
    case BlockType::Free: return os << "Free";
    case BlockType::Error: return os << "Error";
    default: {
        return os << "<unknown>";
    }
    }
}

inline ostreamtype& operator<<(ostreamtype& os, const BlockHead &h) {
    return os << "BAS<type=" << h.type << " age=" << h.age << " ts=" << h.timestamp << " link=" << h.linked_block << ">";
}

}

#endif
