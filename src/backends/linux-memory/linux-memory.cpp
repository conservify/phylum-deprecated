#include "linux-memory.h"

namespace confs {

LinuxMemoryBackend::LinuxMemoryBackend() : size_(0), ptr_(nullptr) {
}

bool LinuxMemoryBackend::initialize(confs_geometry_t geometry) {
    geometry_ = geometry;

    return true;
}

bool LinuxMemoryBackend::open() {
    assert(geometry_.valid());

    close();

    size_ = geometry_.number_of_sectors() * geometry_.sector_size;
    ptr_ = (uint8_t *)malloc(size_);

    assert(ptr_ != nullptr);

    log_.append(LogEntry{ OperationType::Opened, ptr_ });

    return true;
}

bool LinuxMemoryBackend::close() {
    if (ptr_ != nullptr) {
        free(ptr_);
        ptr_ = nullptr;
    }
    return true;
}

confs_geometry_t &LinuxMemoryBackend::geometry() {
    return geometry_;
}

size_t LinuxMemoryBackend::size() {
    return size_;
}

bool LinuxMemoryBackend::erase(block_index_t block) {
    assert(geometry_.contains({ block, 0 }));

    auto p = ptr_ + (block * geometry_.block_size());
    memset(p, 0xff, geometry_.block_size());

    log_.append(LogEntry{ OperationType::EraseBlock, block, p });

    return true;
}

bool LinuxMemoryBackend::read(confs_sector_addr_t addr, void *d, size_t n) {
    assert(geometry_.contains(addr));
    assert(n < geometry_.sector_size);

    auto o = addr.block * geometry_.block_size() + (addr.sector * geometry_.sector_size);
    assert(o + n < size_);

    auto p = ptr_ + o;
    memcpy(d, p, n);

    log_.append(LogEntry{ OperationType::Read, addr, p, n });

    return true;
}

static void verify_erased(confs_sector_addr_t addr, uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (*p != 0xff) {
            sdebug << "Corruption: " << addr << std::endl;
            assert(*p == 0xff);
        }
        p++;
    }
}

bool LinuxMemoryBackend::write(confs_sector_addr_t addr, void *d, size_t n) {
    assert(geometry_.contains(addr));
    assert(n < geometry_.sector_size);

    auto o = addr.block * geometry_.block_size() + (addr.sector * geometry_.sector_size);
    assert(o + n < size_);

    auto p = ptr_ + o;
    verify_erased(addr, p, n);
    memcpy(p, d, n);

    log_.append(LogEntry{ OperationType::Write, addr, p, n });

    return true;
}

}