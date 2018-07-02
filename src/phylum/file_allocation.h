#ifndef __PHYLUM_FILE_ALLOCATION_H_INCLUDED
#define __PHYLUM_FILE_ALLOCATION_H_INCLUDED

#include "phylum/private.h"

namespace phylum {

struct Extent {
    block_index_t start;
    block_index_t nblocks;

    bool contains(block_index_t block) const {
        return block >= start && block < start + nblocks;
    }

    bool contains(const BlockAddress &address) const {
        return contains((block_index_t)address.block);
    }

    BlockAddress final_sector(const Geometry &g) const {
        return { start + nblocks - 1, g.block_size() - SectorSize };
    }

    BlockAddress end(const Geometry &g) const {
        return { start + nblocks, 0 };
    }

    bool operator==(const Extent &other) const {
        return start == other.start && nblocks == other.nblocks;
    }

    bool operator!=(const Extent &other) const {
        return !(*this == other);
    }
};

struct FileAllocation {
    Extent index;
    Extent data;

    bool operator==(const FileAllocation &other) const {
        return index == other.index && data == other.data;
    }

    bool operator!=(const FileAllocation &other) const {
        return !(*this == other);
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const Extent &e) {
    return os << "Extent<" << e.start << " - " << e.start + e.nblocks << " l=" << e.nblocks << ">";
}

inline ostreamtype& operator<<(ostreamtype& os, const FileAllocation &f) {
    return os << "FileAllocation<index=" << f.index << " data=" << f.data << ">";
}

}

#endif
