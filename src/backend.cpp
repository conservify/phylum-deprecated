#include "phylum/backend.h"

namespace phylum {

SectorCachingStorage::SectorCachingStorage(StorageBackend &target) : target(target) {
}

bool SectorCachingStorage::read(BlockAddress addr, void *d, size_t n) {
    auto sector = addr.sector(geometry());
    auto offset = addr.sector_offset(geometry());
    if (sector_ != sector) {
        if (!target.read({ sector, 0 }, buffer_, SectorSize)) {
            return false;
        }
        sector_ = sector;
        #if PHYLUM_DEBUG > 3
        sdebug() << "SectorCache: MISS " << sector << endl;
        #endif
    }
    else {
        #if PHYLUM_DEBUG > 3
        sdebug() << "SectorCache: HIT " << sector << endl;
        #endif
    }

    memcpy(d, buffer_ + offset, n);

    return true;
}

bool SectorCachingStorage::write(BlockAddress addr, void *d, size_t n) {
    auto sector = addr.sector(geometry());
    auto offset = addr.sector_offset(geometry());
    if (sector_ != sector) {
        return target.write(addr, d, n);
    }

    memcpy(buffer_ + offset, d, n);

    return target.write({ sector, 0 }, buffer_, SectorSize);
}

}
