#include "phylum/backend.h"

namespace phylum {

SectorCachingStorage::SectorCachingStorage(StorageBackend &target) : target(target) {
}

bool SectorCachingStorage::read(BlockAddress addr, void *d, size_t n) {
    auto sector = addr.sector(geometry());
    auto offset = addr.sector_offset(geometry());
    auto sector_size = geometry().sector_size;
    if (sector_ != sector) {
        #if PHYLUM_DEBUG > 3
        sdebug() << "SectorCache: MISS " << addr << endl;
        #endif
        if (!target.read({ geometry(), sector, 0 }, buffer_, sector_size)) {
            return false;
        }
        sector_ = sector;
    }
    else {
        #if PHYLUM_DEBUG >= 3
        sdebug() << "SectorCache: HIT " << addr << endl;
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

    return target.write({ geometry(), sector, 0 }, buffer_, SectorSize);
}

}
