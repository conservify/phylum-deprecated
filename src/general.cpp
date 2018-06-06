#include <confs/confs.h>
#include <confs/private.h>

namespace confs {

std::ostream &sdebug = std::cout;

constexpr char BlockMagic::MagicKey[];

void BlockMagic::fill() {
    memcpy(key, MagicKey, sizeof(MagicKey));
}

bool BlockMagic::valid() const {
    return memcmp(key, MagicKey, sizeof(MagicKey)) == 0;
}

}
