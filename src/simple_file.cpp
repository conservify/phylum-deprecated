#include <algorithm>

#include "phylum/phylum.h"
#include "phylum/simple_file.h"
#include "size_calcs.h"

namespace phylum {

bool SimpleFile::seek(uint64_t desired) {
    IndexRecord end;
    if (!index().seek(desired, end)) {
        return false;
    }

    if (end.valid()) {
        head_ = end.address;
        position_ = end.position;
    }
    else {
        head_ = { file_->data.start, 0 };
        length_ = 0;
        version_ = 1;
        return true;
    }

    auto info = seek(head_.block, desired - end.position);

    seek_offset_ = info.address.sector_offset(geometry());
    version_ = info.version;
    head_ = info.address;
    head_.add(-seek_offset_);

    blocks_since_save_ = info.blocks;
    bytes_in_block_ = info.bytes_in_block;
    position_ += info.bytes;
    if (desired == UINT64_MAX) {
        length_ = end.position + info.bytes;
    }

    #if PHYLUM_DEBUG > 1
    sdebug() << "Seek: length=" << length_ << " position=" << position_ << " desired=" << desired <<
                " endp=" << end.position << " info=" << info.bytes << " head=" << head_ << " start=" << file_->data.start << endl;
    #endif

    return true;
}

SimpleFile::SeekInfo SimpleFile::seek(block_index_t starting_block, uint64_t max, bool verify_head_block) {
    auto bytes = 0;
    auto bytes_in_block = 0;
    auto blocks = 0;
    auto version = (uint32_t)1;

    // This is used just to sanity check that the block we were given has
    // actually been begun. For example, the very first block won't have been.
    if (verify_head_block) {
        FileBlockHead head;
        if (!storage_->read({ starting_block, 0 }, &head, sizeof(FileBlockHead))) {
            return { };
        }

        if (!head.valid()) {
            return { { starting_block, 0 }, version, 0, 0 };
        }

        version = head.version;
    }

    // Start walking the file from the given starting block until we reach the
    // end of the file or we've passed `max` bytes.
    auto &g = geometry();
    auto addr = BlockAddress::tail_sector_of(starting_block, g);
    auto scanned_block = false;
    while (true) {
        if (!storage_->read(addr, buffer_, sizeof(buffer_))) {
            return { };
        }

        // Check to see if our desired location is in this block, otherwise we
        // can just skip this one entirely.
        if (addr.tail_sector(g)) {
            FileBlockTail tail;
            memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
            if (is_valid_block(tail.block.linked_block) && max > tail.bytes_in_block) {
                addr = BlockAddress::tail_sector_of(tail.block.linked_block, g);
                bytes += tail.bytes_in_block;
                max -= tail.bytes_in_block;
                bytes_in_block = 0;
                blocks++;
            }
            else {
                if (scanned_block) {
                    break;
                }
                scanned_block = true;
                bytes_in_block = 0;
                addr = BlockAddress{ addr.block, SectorSize };
            }
        }
        else {
            FileSectorTail tail;
            memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));

            if (tail.bytes == 0 || tail.bytes == SECTOR_INDEX_INVALID) {
                break;
            }
            if (max >= tail.bytes) {
                bytes += tail.bytes;
                bytes_in_block += tail.bytes;
                max -= tail.bytes;
                addr.add(SectorSize);
            }
            else {
                bytes += max;
                bytes_in_block += max;
                addr.add(max);
                break;
            }
        }
    }

    return { addr, version, bytes, bytes_in_block, blocks };
}

int32_t SimpleFile::read(uint8_t *ptr, size_t size) {
    assert(read_only());

    // Are we out of data to return?
    if (buffavailable_ == buffpos_) {
        buffpos_ = 0;
        buffavailable_ = 0;

        // We set head_ to the end of the data extent if we've read to the end.
        if (file_->data.end(geometry()) == head_) {
            return 0;
        }

        // If the head is invalid, seek to the beginning of the file.
        if (!head_.valid()) {
            if (!seek(0)) {
                return 0;
            }
        }

        // Skip head sector, just in case.
        if (head_.beginning_of_block()) {
            head_.add(SectorSize);
        }

        if (!storage_->read(head_, buffer_, sizeof(buffer_))) {
            return 0;
        }

        // See how much data we have in this sector and/or if we have a block we
        // should be moving onto after this sector is read. This advances
        // things for the following reading.
        if (tail_sector()) {
            FileBlockTail tail;
            memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
            buffavailable_ = tail.sector.bytes;
            if (tail.block.linked_block != BLOCK_INDEX_INVALID) {
                head_ = BlockAddress{ tail.block.linked_block, SectorSize };
            }
            else {
                // We should be in the last sector of the file.
                // assert(file_->data.final_sector(geometry()) == head_);
                head_ = file_->data.end(geometry());
            }
        }
        else {
            FileSectorTail tail;
            memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));
            buffavailable_ = tail.bytes;
            head_.add(SectorSize);
        }

        // End of the file? Marked by an "unwritten" sector.
        if (buffavailable_ == 0 || buffavailable_ == SECTOR_INDEX_INVALID) {
            buffavailable_ = 0;
            length_ = position_;
            return 0;
        }

        // Take care of seeks ending in the middle of a block.
        if (seek_offset_ > 0) {
            buffpos_ = seek_offset_;
            seek_offset_ = 0;
        }
    }

    auto remaining = (uint16_t)(buffavailable_ - buffpos_);
    auto copying = remaining > size ? size : remaining;
    memcpy(ptr, buffer_ + buffpos_, copying);

    buffpos_ += copying;
    position_ += copying;

    return copying;
}

int32_t SimpleFile::write(uint8_t *ptr, size_t size, bool span_sectors, bool span_blocks) {
    auto to_write = size;
    auto wrote = 0;

    assert(!read_only());

    // All 'atomic' writes have to be smaller than the smallest sector we can
    // write, which in our case is the block tail sector since that header is large.
    if (!span_sectors) {
        assert(size <= SectorSize - sizeof(FileBlockTail));
    }

    // If the head is invalid, seek to the end of the file.
    if (!head_.valid()) {
        return 0;
    }

    // Don't let writes span blocks.
    if (!span_blocks && bytes_in_block_ > 0) {
        auto block_size = effective_file_block_size(storage_->geometry());
        auto remaining_in_block = block_size - bytes_in_block_;
        if (remaining_in_block < size) {
            if (flush() == 0) {
                return 0;
            }

            // If the head is invalid, seek to the end of the file.
            if (!head_.valid()) {
                return 0;
            }
        }
    }

    while (to_write > 0) {
        auto overhead = tail_sector() ? sizeof(FileBlockTail) : sizeof(FileSectorTail);
        auto remaining = sizeof(buffer_) - overhead - buffpos_;
        auto copying = to_write > remaining ? remaining : to_write;

        if (!span_sectors) {
            if (copying != size) {
                if (flush() == 0) {
                    return wrote;
                }
                continue;
            }
        }

        if (remaining == 0) {
            if (flush() == 0) {
                return wrote;
            }

            // If we're at the end then don't try and write more.
            if (!head_.valid()) {
                return wrote;
            }
        }
        else {
            memcpy(buffer_ + buffpos_, (const uint8_t *)ptr + wrote, copying);
            buffpos_ += copying;
            wrote += copying;
            length_ += copying;
            position_ += copying;
            bytes_in_block_ += copying;
            to_write -= copying;
        }
    }

    if (buffpos_ > 0) {
        if (mode_ == OpenMode::MultipleWrites) {
            save_sector(false);
        }
    }

    return wrote;
}

SimpleFile::SavedSector SimpleFile::save_sector(bool flushing) {
    assert(!read_only());
    assert(buffpos_ > 0 && buffavailable_ == 0);

    // If this is the tail sector in the block write the tail section that links
    // to the following block.
    auto linked = BLOCK_INDEX_INVALID;
    auto writing_tail_sector = tail_sector();
    auto following = head_;

    if (writing_tail_sector) {
        if (flushing) {
            // Check to see if we're at the end of our allocated space.
            linked = head_.block + 1;
            if (!file_->data.contains(linked)) {
                linked = BLOCK_INDEX_INVALID;
            }
        }

        FileBlockTail tail;
        tail.sector.bytes = buffpos_;
        tail.bytes_in_block = bytes_in_block_;
        tail.block.linked_block = linked;
        memcpy(tail_info<FileBlockTail>(buffer_), &tail, sizeof(FileBlockTail));
        following = { linked };
    }
    else {
        FileSectorTail tail;
        tail.bytes = buffpos_;
        memcpy(tail_info<FileSectorTail>(buffer_), &tail, sizeof(FileSectorTail));
        following.add(SectorSize);

        // Write this full sector. No partial writes here because of the tail. Most
        // of the time we write full sectors anyway.
        assert(file_->data.contains(following));
    }

    if (!storage_->write(head_, buffer_, sizeof(buffer_))) {
        return SavedSector{ 0, head_ };
    }

    return SavedSector{ buffpos_, following };
}

int32_t SimpleFile::flush() {
    if (read_only()) {
        return 0;
    }

    if (buffpos_ == 0 || buffavailable_ > 0) {
        return 0;
    }

    auto saved = save_sector(true);
    if (!saved) {
        return 0;
    }

    // We could do this in the if scope above, I like doing things "in order" though.
    if (head_.block != saved.head.block) {
        auto linked = saved.head.block;
        if (is_valid_block(linked)) {
            head_ = initialize(linked, head_.block);
            assert(file_->data.contains(head_));
            if (!head_.valid()) {
                assert(false); // TODO: Yikes.
            }

            // Every N blocks we save our offset in the tree. This affects how much
            // seeking needs to happen when, seeking.
            blocks_since_save_++;
            if (blocks_since_save_ >= (int8_t)IndexFrequency) {
                if (!index(head_)) {
                    return 0;
                }
            }
        }
        else {
            head_ = BlockAddress{ };
        }

        bytes_in_block_ = 0;
    }
    else {
        head_ = saved.head;
    }

    auto flushed = buffpos_;
    buffpos_ = 0;
    return flushed;
}

bool SimpleFile::initialize() {
    length_ = 0;
    position_ = 0;
    buffpos_ = 0;
    buffavailable_ = 0;
    seek_offset_ = 0;
    bytes_in_block_ = 0;
    blocks_since_save_ = 0;

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
    if (!initialize()) {
        sdebug() << "Initialize failed during erase" << endl;
    }

    return format();
}

bool SimpleFile::format() {
    version_++;

    if (!index_.format()) {
        return false;
    }

    head_ = initialize(file_->data.start, BLOCK_INDEX_INVALID);

    if (!initialize()) {
        return false;
    }

    if (!index(head_)) {
        return false;
    }

    return true;
}

FileDescriptor &SimpleFile::fd() const {
    return *fd_;
}

bool SimpleFile::in_final_block() const {
    return (head_.block + 1) == file_->data.start + file_->data.nblocks;
}

uint64_t SimpleFile::maximum_size() const {
    return file_->data.nblocks * effective_file_block_size(geometry());
}

uint64_t SimpleFile::size() const {
    return length_;
}

uint64_t SimpleFile::tell() const {
    return position_;
}

BlockAddress SimpleFile::head() const {
    return head_;
}

FileIndex &SimpleFile::index() {
    return index_;
}

bool SimpleFile::index(BlockAddress address) {
    if (!index().append(length_, head_)) {
        return false;
    }
    blocks_since_save_ = 0;
    return true;
}

BlockAddress SimpleFile::initialize(block_index_t block, block_index_t previous) {
    FileBlockHead head;

    head.fill();
    head.file_id = id_;
    head.version = version_;
    head.block.linked_block = previous;

    if (!storage_->erase(block)) {
        return { };
    }

    if (!storage_->write({ block, 0 }, &head, sizeof(FileBlockHead))) {
        return { };
    }

    return BlockAddress { block, SectorSize };
}

}
