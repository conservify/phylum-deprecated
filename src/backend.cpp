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
