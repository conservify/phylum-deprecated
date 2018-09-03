#include <algorithm>

#include "phylum/phylum.h"
#include "phylum/blocked_file.h"
#include "size_calcs.h"

namespace phylum {

bool AllocatedBlockedFile::preallocate(uint32_t expected_size) {
    return allocator_->preallocate(expected_size);
}

AllocatedBlock AllocatedBlockedFile::allocate() {
    return allocator_->allocate(BlockType::File);
}

void AllocatedBlockedFile::free(block_index_t block) {
    allocator_->free(block, 0);
}

bool BlockedFile::seek(uint64_t desired) {
    auto from = beg_;
    if (!from.valid()) {
        from = head_;
    }
    return seek(from, 0, desired, nullptr);
}

bool BlockedFile::seek(BlockAddress from, uint32_t position_at_from, uint64_t desired, BlockVisitor *visitor) {
    auto info = seek(from, position_at_from, desired - position_at_from, visitor, true);
    if (!info.address.valid()) {
        return false;
    }

    seek_offset_ = info.address.sector_offset(geometry());
    version_ = info.version;
    head_ = info.address;
    head_.add(-seek_offset_);

    blocks_in_file_ = info.blocks;
    bytes_in_block_ = info.bytes_in_block;
    position_ += info.bytes;

    auto success = true;
    if (desired == UINT64_MAX) {
        length_ = position_at_from + info.bytes;
    }
    else {
        success = position_ == desired;
    }

    #if PHYLUM_DEBUG > 1
    sdebug() << "Seek: length=" << length_ << " position=" << position_ << " desired=" << desired <<
        " endp=" << position_at_from << " info=" << info.bytes << " head=" << head_ << " seek_offset=" << seek_offset_ << endl;
    #endif

    return success;
}

BlockedFile::SeekInfo BlockedFile::seek(BlockAddress from, uint32_t position_at_from, uint64_t desired, BlockVisitor *visitor, bool verify_head_block) {
    auto bytes = 0;
    auto bytes_in_block = 0;
    auto blocks = 0;
    auto version = (uint32_t)1;
    auto starting_block = from.block;

    if (from.valid()) {
        head_ = from;
        position_ = position_at_from;
    }
    else {
        assert(false);
        return { { }, 0, 0, 0, 0 };
    }


    // This is used just to sanity check that the block we were given has
    // actually been begun. For example, the very first block won't have been.
    if (verify_head_block) {
        FileBlockHead head;
        if (!storage_->read({ starting_block, 0 }, &head, sizeof(FileBlockHead))) {
            return { };
        }

        if (!head.valid()) {
            return { };
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
            auto this_block = addr.block;

            FileBlockTail tail;
            memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
            if (is_valid_block(tail.block.linked_block) && desired >= tail.bytes_in_block) {
                addr = BlockAddress::tail_sector_of(tail.block.linked_block, g);
                bytes += tail.bytes_in_block;
                desired -= tail.bytes_in_block;
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

            if (visitor != nullptr) {
                visitor->block(this_block);
            }
        }
        else {
            FileSectorTail tail;
            memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));

            if (tail.bytes == 0 || tail.bytes == SECTOR_INDEX_INVALID) {
                break;
            }
            if (desired >= tail.bytes) {
                bytes += tail.bytes;
                bytes_in_block += tail.bytes;
                desired -= tail.bytes;
                addr.add(SectorSize);
            }
            else {
                bytes += desired;
                bytes_in_block += desired;
                addr.add(desired);
                break;
            }
        }
    }

    return { addr, version, bytes, bytes_in_block, blocks };
}

bool BlockedFile::walk(BlockVisitor *visitor) {
    if (!seek(0)) {
        return false;
    }

    if (!seek(head_, 0, UINT64_MAX, visitor)) {
        return false;
    }

    return true;
}

int32_t BlockedFile::read(uint8_t *ptr, size_t size) {
    assert(read_only());

    // Are we out of data to return?
    if (buffavailable_ == buffpos_) {
        buffpos_ = 0;
        buffavailable_ = 0;

        // We set head_ to the end of the data extent if we've read to the end.
        if (head_ == end_of_file()) {
            return 0;
        }

        // If the head is invalid, seek to the beginning of the file.
        if (!head_.valid()) {
            if (!seek(0)) {
                return 0;
            }
        }

        // Skip head sector, just in case.
        if (head_.is_beginning_of_block()) {
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
                head_ = end_of_file();
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

int32_t BlockedFile::write(uint8_t *ptr, size_t size, bool span_sectors, bool span_blocks) {
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

BlockedFile::SavedSector BlockedFile::save_sector(bool flushing) {
    assert(!read_only());
    assert(buffpos_ > 0 && buffavailable_ == 0);

    // If this is the tail sector in the block write the tail section that links
    // to the following block.
    auto linked = BLOCK_INDEX_INVALID;
    auto writing_tail_sector = tail_sector();
    auto following = head_;
    auto allocated = AllocatedBlock{ };

    if (writing_tail_sector) {
        if (flushing) {
            // Check to see if we're at the end of our allocated space.
            allocated = allocate();
            linked = allocated.block;
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
        // TODO: From SimpleFile
        // assert(file_->data.contains(following));
    }

    if (!storage_->write(head_, buffer_, sizeof(buffer_))) {
        return SavedSector{ 0, head_, allocated };
    }

    return SavedSector{ buffpos_, following, allocated };
}

int32_t BlockedFile::flush() {
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
        if (saved.allocated.valid() && is_valid_block(linked)) {
            head_ = initialize(saved.allocated, head_.block);
            // TODO: From SimpleFile
            // assert(file_->data.contains(head_));
            if (!head_.valid()) {
                assert(false); // TODO: Yikes.
            }

            // Every N blocks we save our offset in the tree. This affects how much
            // seeking needs to happen when, seeking.
            blocks_in_file_++;
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

bool BlockedFile::initialize() {
    length_ = 0;
    position_ = 0;
    buffpos_ = 0;
    buffavailable_ = 0;
    seek_offset_ = 0;
    bytes_in_block_ = 0;
    blocks_in_file_ = 0;

    return true;
}

bool BlockedFile::erase_all_blocks() {
    class Eraser : public BlockVisitor {
    private:
        BlockedFile *file_;

    public:
        Eraser(BlockedFile *file) : file_(file) {
        }

    public:
        void block(block_index_t block) override {
            file_->free(block);
        }
    };

    Eraser eraser(this);

    if (!seek(beg_, 0, UINT64_MAX, &eraser)) {
    }

    return erase();
}

bool BlockedFile::erase() {
    if (!initialize()) {
        return false;
    }

    if (!format()) {
        return false;
    }

    return true;
}

bool BlockedFile::format() {
    version_++;

    if (!initialize()) {
        return false;
    }

    head_ = initialize(allocate(), BLOCK_INDEX_INVALID);
    beg_ = head_.beginning_of_block();

    return true;
}

bool BlockedFile::read_only() const {
    return mode_ == OpenMode::Read;
}

uint32_t BlockedFile::blocks_in_file() const {
    return blocks_in_file_;
}

uint64_t BlockedFile::size() const {
    return length_;
}

uint64_t BlockedFile::tell() const {
    return position_;
}

BlockAddress BlockedFile::beginning() const {
    return beg_;
}

BlockAddress BlockedFile::head() const {
    return head_;
}

BlockAddress BlockedFile::end_of_file() {
    return BlockAddress { };
}

uint32_t BlockedFile::version() const {
    return version_;
}

void BlockedFile::close() {
    flush();
}

bool BlockedFile::exists() {
    if (!head_.valid()) {
        return false;
    }

    FileBlockHead head;
    if (!storage_->read(head_, &head, sizeof(FileBlockHead))) {
        return false;
    }

    if (!head.valid()) {
        return false;
    }

    beg_ = head_;

    return true;
}

BlockAddress BlockedFile::initialize(AllocatedBlock allocated, block_index_t previous) {
    FileBlockHead head;

    head.fill();
    head.file_id = id_;
    head.version = version_;
    head.block.age = allocated.age;
    head.block.linked_block = previous;

    if (!allocated.erased) {
        if (!storage_->erase(allocated.block)) {
            return { };
        }
    }

    if (!storage_->write({ allocated.block, 0 }, &head, sizeof(FileBlockHead))) {
        return { };
    }

    return BlockAddress { allocated.block, SectorSize };
}

}
