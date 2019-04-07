#ifndef __PHYLUM_LINUX_MEMORY_H_INCLUDED
#define __PHYLUM_LINUX_MEMORY_H_INCLUDED

#ifndef ARDUINO

#include <phylum/phylum.h>
#include <phylum/private.h>
#include <phylum/backend.h>

#include "debug_log.h"

namespace phylum {

enum class VerificationMode {
    ErasedOnly,
    Appending
};

class LinuxMemoryBackend : public StorageBackend {
private:
    StorageLog log_;
    Geometry geometry_;
    uint64_t size_;
    uint8_t *ptr_;
    VerificationMode verification_{ VerificationMode::ErasedOnly };

public:
    static uint8_t EraseByte;

public:
    LinuxMemoryBackend();

public:
    uint64_t size() {
        return size_;
    }

    StorageLog &log() {
        return log_;
    }

    VerificationMode verification() {
        return verification_;
    }

    void verification(VerificationMode mode) {
        verification_ = mode;
    }

public:
    bool initialize(Geometry geometry);

public:
    bool open() override;
    bool close() override;
    Geometry &geometry() override;
    void geometry(Geometry g) override;
    bool erase(block_index_t block) override;
    void randomize();
    bool read(BlockAddress addr, void *d, size_t n) override;
    bool write(BlockAddress addr, void *d, size_t n) override;

};

}

#endif // ARDUINO

#endif // __PHYLUM_LINUX_MEMORY_H_INCLUDED
