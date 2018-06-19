#include "phylum/phylum.h"
#include "phylum/preallocated.h"

namespace phylum {

static EmptyAllocator empty_allocator;

class ExtentAllocator : public Allocator {
private:
    Extent extent_;
    block_index_t block_;

public:
    ExtentAllocator(Extent extent, block_index_t block) : extent_(extent), block_(block) {
    }

public:
    virtual block_index_t allocate(BlockType type) override {
        return block_++;
    }

};

static uint64_t file_block_overhead(const Geometry &geometry) {
    auto sectors_per_block = geometry.sectors_per_block();
    return SectorSize + sizeof(FileBlockTail) + ((sectors_per_block - 2) * sizeof(FileSectorTail));
}

static uint64_t effective_file_block_size(const Geometry &geometry) {
    return geometry.block_size() - file_block_overhead(geometry);
}

static uint64_t index_block_overhead(const Geometry &geometry) {
    return SectorSize + sizeof(IndexBlockTail);
}

static uint64_t effective_index_block_size(const Geometry &geometry) {
    return geometry.block_size() - index_block_overhead(geometry);
}

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage, Allocator &allocator, BlockAddress address) {
    return { storage, allocator, address, BlockType::Index };
}

static inline BlockLayout<IndexBlockHead, IndexBlockTail> get_index_layout(StorageBackend &storage, BlockAddress address) {
    return get_index_layout(storage, empty_allocator, address);
}

template<typename T, size_t N>
static T *tail_info(uint8_t(&buffer)[N]) {
    auto tail_offset = sizeof(buffer) - sizeof(T);
    return reinterpret_cast<T*>(buffer + tail_offset);
}

bool FileIndex::format() {
    if (!storage_->erase(address().block)) {
        return false;
    }
    return true;
}

bool FileIndex::initialize() {
    if (head_.valid()) {
        return true;
    }

    auto layout = get_index_layout(*storage_, address());

    IndexRecord record;
    while (layout.walk<IndexRecord>(record)) {
        if (record.version > version_) {
            version_ = record.version;
        }
    }

    head_ = address();
    if (layout.find_append_location<IndexRecord>(head_.block)) {
        head_ = layout.address();
    }

    return true;
}

bool FileIndex::seek(uint64_t position, IndexRecord &selected) {
    if (!initialize()) {
        return false;
    }

    auto reading = get_index_layout(*storage_, address());

    IndexRecord record;
    while (reading.walk<IndexRecord>(record)) {
        if (position == record.position) {
            selected = record;
            return true;
        }
        else if (record.position > position) {
            return true;
        }

        selected = record;
    }

    return true;
}

FileIndex::ReindexInfo FileIndex::append(uint32_t position, BlockAddress address) {
    if (!initialize()) {
        return { };
    }

    if (version_ >= 1) {
        return reindex(position, address);
    }

    auto allocator = ExtentAllocator{ file_->index_, head_.block };
    auto layout = get_index_layout(*storage_, allocator, head_);

    auto record = IndexRecord{ position, address, version_ };
    if (!layout.append(record)) {
        return { };
    }

    head_ = layout.address();

    // sdebug() << "Append: " << record << " head=" << head_ << std::endl;

    return { position, 0 };
}

FileIndex::ReindexInfo FileIndex::reindex(uint64_t length, BlockAddress new_end) {
    version_++;
    head_.invalid();

    auto reading = get_index_layout(*storage_, address());
    auto writing = get_index_layout(*storage_, address());

    // sdebug() << "Reindex: " << reading.address() << " -> " << writing.address() << " length = " << length << " end = " << new_end << std::endl;

    uint64_t offset = 0;
    IndexRecord record;
    while (reading.walk<IndexRecord>(record)) {
        // sdebug() << "  Old: " << record << std::endl;
        if (record.version == version_ - 1) {
            if (record.position == 0) {
                // If offset is non-zero then we've looped around.
                if (offset != 0) {
                    break;
                }
            }
            else {
                if (offset == 0) {
                    offset = record.position;
                }

                auto nrecord = IndexRecord{ record.position - offset, record.address, version_ };
                // sdebug() << "  " << nrecord << std::endl;
                if (!writing.append(nrecord)) {
                    return { };
                }
            }
        }
    }

    auto new_length = length - offset;

    auto nrecord = IndexRecord{ new_length, new_end, version_ };
    // sdebug() << "  " << nrecord << std::endl;
    if (!writing.append(nrecord)) {
        return { };
    }

    // sdebug() << "EoI: length = " << new_length << std::endl;

    return { new_length, offset };
}

void FileIndex::dump() {
    auto layout = get_index_layout(*storage_, address());

    sdebug() << "Index: " << address() << endl;

    IndexRecord record;
    while (layout.walk<IndexRecord>(record)) {
        sdebug() << "  " << record << endl;
    }
}

bool SimpleFile::seek(uint64_t position) {
    IndexRecord end;
    if (!index().seek(position, end)) {
        return false;
    }
    if (end.valid()) {
        head_ = end.address;
        position_ = end.position;
    }
    else {
        head_ = { file_->data_.start, 0 };
        return true;
    }

    auto info = seek(head_.block, position - end.position);

    seek_offset_ = info.address.sector_offset(geometry());
    head_ = info.address;
    head_.add(-seek_offset_);
    blocks_since_save_ = info.blocks;
    position_ += info.bytes;
    if (position == UINT64_MAX) {
        length_ = end.position + info.bytes;
    }

    return true;
}

int32_t SimpleFile::read(uint8_t *ptr, size_t size) {
    // Are we out of data to return?
    if (buffavailable_ == buffpos_) {
        buffpos_ = 0;
        buffavailable_ = 0;

        // We set head_ to the end of the data extent if we've read to the end.
        if (file_->data_.end(geometry()) == head_) {
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
                assert(file_->data_.final_sector(geometry()) == head_);
                head_ = file_->data_.end(geometry());
                assert(file_->data_.end(geometry()) == head_);
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
            // If we're at the end we know our length.
            if (length_ == ((uint32_t)-1)) {
                assert(false);
                length_ = position_;
            }
            buffavailable_ = 0;
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

int32_t SimpleFile::write(uint8_t *ptr, size_t size) {
    auto to_write = size;
    auto wrote = 0;

    assert(!readonly_);

    // If the head is invalid, seek to the end of the file.
    if (!head_.valid()) {
        if (!seek(UINT64_MAX)) {
            return 0;
        }

        if (!head_.valid()) {
            return 0;
        }
    }

    while (to_write > 0) {
        auto overhead = tail_sector() ? sizeof(FileBlockTail) : sizeof(FileSectorTail);
        auto remaining = sizeof(buffer_) - overhead - buffpos_;
        auto copying = to_write > remaining ? remaining : to_write;

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

    return wrote;
}

int32_t SimpleFile::flush() {
    if (readonly_) {
        return 0;
    }

    if (buffpos_ == 0 || buffavailable_ > 0) {
        return 0;
    }

    // If this is the tail sector in the block write the tail section that links
    // to the following block.
    auto linked = BLOCK_INDEX_INVALID;
    auto writing_tail_sector = tail_sector();
    auto addr = head_;
    if (writing_tail_sector) {
        // Check to see if we're at the end of our allocated space.
        linked = head_.block + 1;
        if (!file_->data_.contains(linked)) {
            switch (file_->fd_->strategy) {
            case WriteStrategy::Append: {
                linked = BLOCK_INDEX_INVALID;
                break;
            }
            case WriteStrategy::Rolling: {
                linked = rollover();
                break;
            }
            }
        }

        FileBlockTail tail;
        tail.sector.bytes = buffpos_;
        tail.bytes_in_block = bytes_in_block_;
        tail.block.linked_block = linked;
        memcpy(tail_info<FileBlockTail>(buffer_), &tail, sizeof(FileBlockTail));
    }
    else {
        FileSectorTail tail;
        tail.bytes = buffpos_;
        memcpy(tail_info<FileSectorTail>(buffer_), &tail, sizeof(FileSectorTail));
        head_.add(SectorSize);
    }

    // Write this full sector. No partial writes here because of the tail. Most
    // of the time we write full sectors anyway.
    assert(file_->data_.contains(addr));

    if (!storage_->write(addr, buffer_, sizeof(buffer_))) {
        return 0;
    }

    // We could do this in the if scope above, I like doing things "in order" though.
    if (writing_tail_sector) {
        if (is_valid_block(linked)) {
            head_ = initialize(linked, head_.block);
            assert(file_->data_.contains(head_));
            if (!head_.valid()) {
                assert(false); // TODO: Yikes.
            }

            // Every N blocks we save our offset in the tree. This affects how much
            // seeking needs to happen when, seeking.
            blocks_since_save_++;
            if (blocks_since_save_ == IndexFrequency) {
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

    auto flushed = buffpos_;
    buffpos_ = 0;
    return flushed;
}

bool SimpleFile::initialize() {
    return true;
}

bool SimpleFile::format() {
    if (!index_.format()) {
        return false;
    }

    head_ = initialize(file_->data_.start, BLOCK_INDEX_INVALID);
    if (!index(head_)) {
        return false;
    }

    return true;
}

uint64_t SimpleFile::maximum_size() const {
    return file_->data_.nblocks * effective_file_block_size(geometry());
}

uint64_t SimpleFile::size() const {
    return length_;
}

uint64_t SimpleFile::tell() const {
    return position_;
}

uint32_t SimpleFile::truncated() const {
    return truncated_;
}

FileIndex &SimpleFile::index() {
    return index_;
}

bool SimpleFile::index(BlockAddress address) {
    auto info = index().append(length_, head_);
    blocks_since_save_ = 0;
    length_ = info.length;
    position_ = info.length;
    truncated_ += info.truncated;
    return true;
}

block_index_t SimpleFile::rollover() {
    auto info = index().reindex(length_, { file_->data_.start, SectorSize });
    if (!info) {
        return BLOCK_INDEX_INVALID;
    }

    blocks_since_save_ = -1; // HACK
    length_ = info.length;
    position_ = info.length;
    truncated_ += info.truncated;

    return file_->data_.start;
}

SimpleFile::SeekInfo SimpleFile::seek(block_index_t starting_block, uint64_t max, bool verify_head_block) {
    auto bytes = 0;
    auto blocks = 0;

    // This is used just to sanity check that the block we were given has
    // actually been begun. For example, the very first block won't have been.
    if (verify_head_block) {
        FileBlockHead head;
        if (!storage_->read({ starting_block, 0 }, &head, sizeof(FileBlockHead))) {
            return { };
        }

        if (!head.valid()) {
            return { { starting_block, 0 }, 0, 0 };
        }
    }

    // Start walking the file from the given starting block until we reach the
    // end of the file or we've passed `max` bytes.
    auto &g = geometry();
    auto addr = BlockAddress::tail_sector_of(starting_block, g);
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
                blocks++;
            }
            else {
                addr = BlockAddress{ addr.block, SectorSize };
            }
        }
        else {
            FileSectorTail tail;
            memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));

            if (tail.bytes == 0 || tail.bytes == SECTOR_INDEX_INVALID) {
                break;
            }
            if (max > tail.bytes) {
                bytes += tail.bytes;
                max -= tail.bytes;
                addr.add(SectorSize);
            }
            else {
                bytes += max;
                addr.add(max);
                break;
            }
        }
    }

    return { addr, bytes, blocks };
}

BlockAddress SimpleFile::initialize(block_index_t block, block_index_t previous) {
    FileBlockHead head;

    head.fill();
    head.file_id = id_;
    head.block.linked_block = previous;

    if (!storage_->erase(block)) {
        return { };
    }

    if (!storage_->write({ block, 0 }, &head, sizeof(FileBlockHead))) {
        return { };
    }

    return BlockAddress { block, SectorSize };
}

bool FilePreallocator::allocate(uint8_t id, FileDescriptor *fd, File &file) {
    auto nblocks = block_index_t(0);
    auto index_blocks = block_index_t(0);

    assert(fd != nullptr);

    if (fd->maximum_size > 0) {
        nblocks = blocks_required_for_data(fd->maximum_size);
        index_blocks = blocks_required_for_index(nblocks) * 2;
    }
    else {
        nblocks = geometry().number_of_blocks - head_ - 1;
        index_blocks = blocks_required_for_index(nblocks) * 2;
        nblocks -= index_blocks;
    }

    assert(nblocks > 0);

    auto index = Extent{ head_, index_blocks };
    head_ += index.nblocks;
    assert(geometry().contains(BlockAddress{ head_, 0 }));

    auto data = Extent{ head_, nblocks };
    head_ += data.nblocks;
    assert(geometry().contains(BlockAddress{ head_, 0 }));

    file = File{ *fd, (uint8_t)id, index, data };

    #ifdef PHYLUM_LAYOUT_DEBUG
    sdebug() << "Allocated: " << file << " " << fd->name << endl;
    #endif

    return true;
}

block_index_t FilePreallocator::blocks_required_for_index(block_index_t nblocks) {
    auto indices_per_block = effective_index_block_size(geometry()) / sizeof(IndexRecord);
    auto indices = (nblocks / 8) + 1;
    return std::max((uint64_t)1, indices / indices_per_block);
}

block_index_t FilePreallocator::blocks_required_for_data(uint64_t opaque_size) {
    constexpr uint64_t Megabyte = (1024 * 1024);
    constexpr uint64_t Kilobyte = (1024);
    uint64_t scale = 0;

    if (geometry().size() < 1024 * Megabyte) {
        scale = Kilobyte;
    }
    else {
        scale = Megabyte;
    }

    auto size = opaque_size * scale;
    return (size / effective_file_block_size(geometry())) + 1;
}

}