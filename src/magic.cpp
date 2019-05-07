#include <cstring>
#include <cassert>

#include "phylum/magic.h"

namespace phylum {

constexpr char BlockMagic::MagicKey[];

BlockMagic::BlockMagic(const char *k) {
    assert(k == MagicKey); // :)
    memcpy(key, k, sizeof(MagicKey));
}

void BlockMagic::fill() {
    memcpy(key, MagicKey, sizeof(MagicKey));
}

bool BlockMagic::valid() const {
    return memcmp(key, MagicKey, sizeof(MagicKey)) == 0;
}

}
