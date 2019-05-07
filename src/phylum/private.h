#ifndef __PHYLUM_PRIVATE_H_INCLUDED
#define __PHYLUM_PRIVATE_H_INCLUDED

#ifndef ARDUINO
#include <iostream>
#include <iomanip>
#endif

#include <cinttypes>
#include <cstdlib>

#include "phylum/platform.h"
#include "phylum/magic.h"
#include "phylum/addressing.h"

namespace phylum {

using block_age_t = uint32_t;
using timestamp_t = uint32_t;
using file_id_t = uint32_t;

constexpr timestamp_t TIMESTAMP_INVALID = ((timestamp_t)-1);
constexpr block_age_t BLOCK_AGE_INVALID = ((block_age_t)-1);
constexpr file_id_t FILE_ID_INVALID = ((file_id_t)-1);

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

inline ostreamtype& operator<<(ostreamtype& os, const BlockTail &e) {
    return os << "BlockTail<linked=" << e.linked_block << ">";
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

#ifndef ARDUINO
inline std::ostream& operator<<(std::ostream& os, const BlockTail &e) {
    return os << "BlockTail<linked=" << e.linked_block << ">";
}

inline std::ostream& operator<<(std::ostream& os, const BlockType &t) {
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

inline std::ostream& operator<<(std::ostream& os, const BlockHead &h) {
    return os << "BAS<type=" << h.type << " age=" << h.age << " ts=" << h.timestamp << " link=" << h.linked_block << ">";
}
#endif

}

#endif // __PHYLUM_PRIVATE_H_INCLUDED
