#include "phylum/files.h"

namespace phylum {

AllocatedBlockedFile Files::open(BlockAddress start, OpenMode mode) {
    return AllocatedBlockedFile{ backend_, mode, allocator_, start };
}

}
