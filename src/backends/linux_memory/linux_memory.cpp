#include "linux_memory.h"

#ifndef ARDUINO

namespace phylum {

uint8_t LinuxMemoryBackend::EraseByte = 0xff;

LinuxMemoryBackend::LinuxMemoryBackend() : size_(0), ptr_(nullptr) {
}

bool LinuxMemoryBackend::initialize(Geometry geometry) {
    geometry_ = geometry;

    return true;
}

void LinuxMemoryBackend::randomize() {
    size_ = geometry_.number_of_sectors() * geometry_.sector_size;
    for (size_t i = 0; i < size_ / sizeof(uint32_t); i++) {
        ((uint32_t *)ptr_)[i] = rand();
    }
}

bool LinuxMemoryBackend::open() {
    assert(geometry_.valid());

    close();

    size_ = (uint64_t)geometry_.number_of_sectors() * geometry_.sector_size;
    ptr_ = (uint8_t *)malloc(size_);
    owner_ = true;
    assert(ptr_ != nullptr);

    // TODO: This helps make tests more deterministic. To simulate garbage in a
    // test, do so explicitly.
    // memset(ptr_, EraseByte, size_);
    randomize();

    log_.logging(false);
    log_.append(LogEntry{ OperationType::Opened, ptr_ });

    return true;
}

bool LinuxMemoryBackend::open(void *ptr, Geometry geometry) {
    assert(geometry.valid());

    close();

    geometry_ = geometry;
    size_ = (uint64_t)geometry_.number_of_sectors() * geometry_.sector_size;
    ptr_ = (uint8_t *)ptr;
    owner_ = false;
    assert(ptr_ != nullptr);

    log_.logging(false);
    log_.append(LogEntry{ OperationType::Opened, ptr_ });

    return true;
}

bool LinuxMemoryBackend::close() {
    if (ptr_ != nullptr) {
        if (owner_) {
            free(ptr_);
        }
        ptr_ = nullptr;
    }
    return true;
}

Geometry &LinuxMemoryBackend::geometry() {
   return geometry_;
}

void LinuxMemoryBackend::geometry(Geometry g) {
    geometry_ = g;
}

bool LinuxMemoryBackend::eraseAll() {
    return false;
}

bool LinuxMemoryBackend::erase(block_index_t block) {
    #if defined(PHYLUM_READ_ONLY)
    assert(false);
    return true;
    #else
    assert(geometry_.contains(BlockAddress{ block, 0 }));

    auto p = ptr_ + ((uint64_t)block * geometry_.block_size());
    memset(p, EraseByte, geometry_.block_size());

    log_.append(LogEntry{ OperationType::EraseBlock, block, p });

    return true;
    #endif
}

bool LinuxMemoryBackend::read(BlockAddress addr, void *d, size_t n) {
    assert(addr.valid());
    assert(geometry_.contains(addr));
    if (strict_sectors_) {
        assert(n <= geometry_.sector_size);
        assert(addr.sector_offset(geometry_) + n <= geometry_.sector_size);
    }

    auto o = (uint64_t)addr.block * geometry_.block_size() + (addr.position);
    assert(o + n <= size_);

    auto p = ptr_ + o;
    memcpy(d, p, n);

    log_.append(LogEntry{ OperationType::Read, addr, p, n });

    return true;
}

#if !defined(PHYLUM_READ_ONLY)
static void verify_erased(BlockAddress addr, uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (*p != LinuxMemoryBackend::EraseByte) {
            sdebug() << "Corruption: " << addr << endl;
            assert(*p == LinuxMemoryBackend::EraseByte);
        }
        p++;
    }
}

static void verify_append(BlockAddress addr, uint8_t *p, uint8_t *src, size_t n) {
    // NOTE: We can't do this, unfortunately, because the block tail headers change.
    /*
    for (size_t i = 0; i < n; ++i) {
        if (*p != LinuxMemoryBackend::EraseByte) {
            if (*p != *src) {
                sdebug() << "Corruption: " << addr << " " << i << endl;
                assert(*p == LinuxMemoryBackend::EraseByte);
            }
        }
        p++;
        src++;
    }
    */
}
#endif

bool LinuxMemoryBackend::write(BlockAddress addr, void *d, size_t n) {
    #if defined(PHYLUM_READ_ONLY)
    assert(false);
    return true;
    #else
    assert(geometry_.contains(addr));
    assert(n <= geometry_.sector_size);
    if (false) {
        assert(addr.sector_offset(geometry_) + n <= geometry_.sector_size);
    }

    auto o = (uint64_t)addr.block * geometry_.block_size() + (addr.position);
    assert(o + n <= size_);

    auto p = ptr_ + o;

    // Do this before the memcpy so that a backup can be made.
    log_.append(LogEntry{ OperationType::Write, addr, p, n });

    switch (verification_) {
    case VerificationMode::ErasedOnly: {
        verify_erased(addr, p, n);
        break;
    }
    case VerificationMode::Appending: {
        verify_append(addr, p, (uint8_t *)d, n);
        break;
    }
    }

    memcpy(p, d, n);

    return true;
    #endif
}

void LinuxMemoryBackend::dump(BlockAddress addr, size_t n) {
    auto o = (uint64_t)addr.block * geometry_.block_size() + (addr.position);
    assert(o + n <= size_);

    auto need_nl = true;

    for (auto i = (size_t)0; i < n; ++i) {
        fprintf(stderr, "%02x ", ptr_[o + i]);
        if ((i + 1) % 32 == 0) {
            fprintf(stderr, "\n");
            need_nl = false;
        }
        else {
            need_nl = true;
        }
    }

    if (need_nl) {
        fprintf(stderr, "\n");
    }
}

}

#endif // ARDUINO
