#include <confs/confs.h>
#include <confs/private.h>

namespace confs {

std::ostream &sdebug = std::cout;

void confs_block_magic_t::fill() {
    memcpy(key, confs_magic_key, sizeof(confs_magic_key));
}

bool confs_block_magic_t::valid() const {
    return memcmp(key, confs_magic_key, sizeof(confs_magic_key)) == 0;
}

}
