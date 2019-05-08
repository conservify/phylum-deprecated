#pragma once

#include <cinttypes>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <phylum/files.h>
#include <phylum/tree_fs_super_block.h>
#include <backends/linux_memory/linux_memory.h>

class PhylumInputStream : public google::protobuf::io::ZeroCopyInputStream {
public:
    PhylumInputStream(phylum::Geometry geometry, uint8_t *everything, phylum::block_index_t block);

    // implements ZeroCopyInputStream ----------------------------------
    bool Next(const void** data, int* size);
    void BackUp(int count);
    bool Skip(int count);
    google::protobuf::int64 ByteCount() const;

public:
    uint32_t position() const {
        return position_;
    }

    phylum::Geometry geometry() const {
        return geometry_;
    }

    phylum::BlockAddress address() const {
        return address_;
    }

    void skip_sector();

private:
    phylum::Geometry geometry_;
    phylum::BlockAddress address_;
    uint8_t *everything_;
    uint8_t *iter_;
    uint64_t position_;
    uint32_t sector_remaining_;

    struct Block {
        uint8_t *ptr;
        size_t size;
    };

    Block previous_block_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(PhylumInputStream);
};
