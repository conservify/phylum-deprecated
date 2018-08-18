#include <algorithm>

#include "phylum/phylum.h"
#include "phylum/simple_file.h"
#include "size_calcs.h"

namespace phylum {

AllocatedBlock ExtentBlockedFile::allocate() {
    auto block = data_.start;
    auto file_head = head();
    if (file_head.valid()) {
        block = file_head.block + 1;

        if (!data_.contains(block)) {
            block = BLOCK_INDEX_INVALID;
        }
    }
    return AllocatedBlock { block, false };
}

void ExtentBlockedFile::free(block_index_t block) {
}

bool SimpleFile::seek(uint64_t desired) {
    IndexRecord end;

    if (!index().seek(desired, end)) {
        return false;
    }

    if (!end.valid()) {
        return blocked_.seek({ file_->data.start, 0 }, 0, desired, nullptr);
    }

    return blocked_.seek(end.address, end.position, desired, nullptr);
}

int32_t SimpleFile::read(uint8_t *ptr, size_t size) {
    return blocked_.read(ptr, size);
}

int32_t SimpleFile::write(uint8_t *ptr, size_t size, bool span_sectors, bool span_blocks) {
    auto written = blocked_.write(ptr, size, span_sectors, span_blocks);
    if (written > 0 && blocked_.blocks_in_file() > 0) {
        if (previous_index_block_ != blocked_.head().block) {
            if ((blocked_.blocks_in_file() % BlockedFile::IndexFrequency) == 0) {
                auto position = tell();
                auto position_at_start_of_block = position - blocked_.bytes_in_block_;
                auto beginning_of_block = blocked_.head().beginning_of_block();

                if (!index().append(position_at_start_of_block, beginning_of_block)) {
                    return false;
                }

                previous_index_block_ = blocked_.head().block;
            }
        }
    }
    return written;
}

bool SimpleFile::initialize() {
    if (!blocked_.initialize()) {
        return false;
    }

    if (!index().initialize()) {
        return false;
    }

    if (!seek(UINT64_MAX)) {
        return false;
    }

    if (read_only()) {
        if (!seek(0)) {
            return false;
        }
    }

    return true;
}

bool SimpleFile::erase() {
    blocked_.initialize();

    if (!index().initialize()) {
        return false;
    }

    if (!seek(0)) {
        return false;
    }

    return format();
}

int32_t SimpleFile::flush() {
    return blocked_.flush();
}

void SimpleFile::close() {
    blocked_.close();
}

bool SimpleFile::format() {
    if (!blocked_.format()) {
        return false;
    }

    if (!index_.format()) {
        return false;
    }

    if (!index().append(0, blocked_.head())) {
        return false;
    }

    return true;
}

FileDescriptor &SimpleFile::fd() const {
    return *fd_;
}

bool SimpleFile::in_final_block() const {
    return (blocked_.head().block + 1) == file_->data.start + file_->data.nblocks;
}

uint64_t SimpleFile::maximum_size() const {
    return file_->data.nblocks * effective_file_block_size(blocked_.geometry());
}

FileIndex &SimpleFile::index() {
    return index_;
}

}
