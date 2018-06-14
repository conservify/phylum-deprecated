#include "phylum/file_system.h"
#include "phylum/stack_node_cache.h"

namespace phylum {

struct FileBlockHeader {
    BlockHead header;
    file_id_t file_id{ FILE_ID_INVALID };

    FileBlockHeader() : header(BlockType::File) {
    }

    void fill() {
        header.magic.fill();
        header.age = 0;
        header.timestamp = 0;
    }

    bool valid() const {
        return header.valid();
    }
};

// NOTE: This is typically ~1700 bytes. With the union between children and
// values in NodeType this weighs in around 1300 bytes.
template<typename NodeType>
struct TreeContext {
public:
    FileSystem &fs;
    NodeSerializer<NodeType> serializer;
    MemoryConstrainedNodeCache<NodeType, 8> cache;
    PersistedTree<NodeType> tree;
    BlockAddress new_head;

public:
    TreeContext(FileSystem &fs) :
        fs(fs), cache(fs.nodes_), tree(cache) {
        tree.head(fs.tree_addr_);
    }

    ~TreeContext() {
        flush();
    }

public:
    void add(uint64_t key, uint64_t value) {
        new_head = tree.add(key, value);
    }

    uint64_t find(uint64_t key) {
        return tree.find(key);
    }

    bool find_less_then(uint64_t key, uint64_t *value, uint64_t *found) {
        return tree.find_less_then(key, value, found);
    }

    void touch() {
        new_head = tree.create_if_necessary();
    }

    bool flush() {
        if (new_head.valid()) {
            // Fill SuperBlock with useful details, save and then kill our
            // new_head so we don't try and save again until a new modification occurs.
            fs.tree_addr_ = new_head;
            fs.prepare(fs.sbm_.block());
            fs.sbm_.save();
            new_head.invalid();
            return true;
        }
        return true;
    }
};

FileSystem::FileSystem(StorageBackend &storage, BlockAllocator &allocator) :
    storage_(&storage), allocator_(&allocator), sbm_{ storage, allocator },
    nodes_{ storage, allocator }, journal_(storage, allocator),
    fpm_(storage, allocator) {
}

void FileSystem::prepare(SuperBlock &sb) {
    // NOTE: This is done again in SuperBlockManager::save
    auto allocator_state = allocator_->state();
    sb.allocator = allocator_state;

    auto tree_state = nodes_.state();
    sb.index = tree_state.index;
    sb.leaf = tree_state.leaf;
    sb.tree = tree_addr_.block;
}

bool FileSystem::initialize(bool wipe) {
    if (!storage_->open()) {
        return false;
    }

    #ifdef PHYLUM_ARDUINO_DEBUG
    sdebug() << "sizeof(TreeContext<NodeType>): " << sizeof(TreeContext<NodeType>) << std::endl;
    #endif

    return open(wipe);
}

bool FileSystem::open(bool wipe) {
    if (wipe || !sbm_.locate()) {
        if (!format()) {
            return false;
        }

        auto &sb = sbm_.block();

        if (!journal_.format(sb.journal)) {
            return false;
        }

        if (!fpm_.format(sb.free)) {
            return false;
        }
    }

    auto &sb = sbm_.block();

    if (!journal_.locate(sb.journal)) {
        return false;
    }

    nodes_.state({ sb.index, sb.leaf });

    tree_addr_ = nodes_.find_head(sb.tree);

    return tree_addr_.valid();
}

bool FileSystem::exists(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto key = INodeKey::file_beginning(name);

    return tc.find(key) != 0;
}

OpenFile FileSystem::open(const char *name, bool readonly) {
    TreeContext<NodeType> tc{ *this };

    auto key = INodeKey::file_beginning(name);
    auto id = key.upper();

    auto existing = tc.find(key);
    if (existing == 0) {
        auto head = initialize_block(allocator_->allocate(BlockType::File), id, BLOCK_INDEX_INVALID);
        tc.add(key, head);
        return { *this, id, head, readonly };
    }

    // TODO: Find a better starting address for the seek.
    auto head = BlockAddress::from(existing);
    auto file = OpenFile { *this, id, head, readonly };
    if (!readonly) {
        file.seek(Seek::End);
    }
    return file;
}

bool FileSystem::touch() {
    TreeContext<NodeType> tc{ *this };
    tc.touch();
    return true;
}

bool FileSystem::format() {
    if (!sbm_.create()) {
        return false;
    }

    if (!sbm_.locate()) {
        return false;
    }

    return touch();
}

bool FileSystem::close() {
    return storage_->close();
}

BlockAddress FileSystem::initialize_block(block_index_t block, file_id_t file_id, block_index_t previous) {
    FileBlockHeader header;

    header.fill();
    header.file_id = file_id;
    header.header.linked_block = previous;

    if (!storage_->erase(block)) {
        return { };
    }

    if (!storage_->write({ block, 0 }, &header, sizeof(FileBlockHeader))) {
        return { };
    }

    return BlockAddress { block, SectorSize };
}

template<typename T, size_t N>
static T *tail_info(uint8_t(&buffer)[N]) {
    auto tail_offset = sizeof(buffer) - sizeof(T);
    return reinterpret_cast<T*>(buffer + tail_offset);
}

OpenFile::OpenFile(FileSystem &fs, file_id_t id, BlockAddress head, bool readonly) :
    fs_(&fs), id_(id), head_(head), readonly_(readonly), length_(readonly ? INVALID_LENGTH : 0) {
    assert(sizeof(buffer_) == SectorSize);
}

bool OpenFile::tail_sector() {
    return head_.tail_sector(fs_->storage().geometry());
}

uint32_t OpenFile::size() {
    if (length_ == INVALID_LENGTH) {
        auto saved = position_;
        seek(Seek::End);
        seek(saved);
    }
    return length_;
}

uint32_t OpenFile::tell() {
    return position_;
}

OpenFile::SeekStatistics OpenFile::seek(BlockAddress starting, uint32_t max) {
    auto bytes = 0;
    auto blocks = 0;

    // Start walking the file from the given starting block until we reach the
    // end of the file or we've passed `max` bytes.
    auto &g = fs_->storage().geometry();
    auto addr = BlockAddress::tail_sector_of(starting.block, g);
    while (true) {
        if (!fs_->storage_->read(addr, buffer_, sizeof(buffer_))) {
            return { };
        }

        // Check to see if our desired location is in this block, otherwise we
        // can just skip this one entirely.
        if (addr.tail_sector(g)) {
            BlockTail tail;
            memcpy(&tail, tail_info<BlockTail>(buffer_), sizeof(BlockTail));
            if (is_valid_block(tail.linked_block) && max > tail.bytes_in_block) {
                addr = BlockAddress::tail_sector_of(tail.linked_block, g);
                bytes += tail.bytes_in_block;
                max -= tail.bytes_in_block;
                blocks++;
            }
            else {
                addr = BlockAddress{ addr.block, SectorSize };
            }
        }
        else {
            SectorTail tail;
            memcpy(&tail, tail_info<SectorTail>(buffer_), sizeof(SectorTail));

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

    return { addr, blocks, bytes };
}

int32_t OpenFile::seek(Seek where, uint32_t position) {
    TreeContext<FileSystem::NodeType> tc{ *fs_ };

    // Easy seek, we can do this directly.
    if (where == Seek::Beginning && position == 0) {
        head_ = BlockAddress::from(tc.find(INodeKey::file_beginning(id_)));
        position_ = 0;
        return 0;
    }

    // This is a little trickier. What we do is look for the first saved INode
    // less than the desired position in the file. If we're seeking to the end
    // then we use UINT32_MAX for the desired position.
    // Technically we could do this faster if we also looked for the following
    // entry and determined if seeking in reverse is a better way. Doesn't seem
    // worth the effort though.
    uint64_t value;
    uint64_t saved;
    auto relative = where == Seek::End ? UINT32_MAX : position;
    assert(tc.find_less_then(INodeKey::file_position(id_, relative), &value, &saved));

    // This gets us pretty close (within POSITION_SAVE_FREQUENCY blocks) so we
    // walk the blocks and sectors to find the actual location.
    auto starting = INodeKey(saved).lower();
    auto ss = seek(BlockAddress::from(value), relative - starting);
    blocks_since_save_ = ss.blocks;
    head_ = ss.address;
    position_ = starting + ss.bytes;

    // If we found the end then remember the length.
    if (where == Seek::End) {
        length_ = position_;
    }

    return position_;
}

int32_t OpenFile::seek(uint32_t position) {
    return seek(Seek::Beginning, position);
}

int32_t OpenFile::write(const void *ptr, size_t size) {
    auto to_write = size;
    auto wrote = 0;

    assert(!readonly_);

    while (to_write > 0) {
        auto overhead = tail_sector() ? sizeof(BlockTail) : sizeof(SectorTail);
        auto remaining = sizeof(buffer_) - overhead - buffpos_;
        auto copying = to_write > remaining ? remaining : to_write;

        if (remaining == 0) {
            if (flush() == 0) {
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

int32_t OpenFile::flush() {
    if (readonly_) {
        return 0;
    }

    if (buffpos_ == 0) {
        return 0;
    }

    // If this is the tail sector in the block write the tail section that links
    // to the following block.
    auto linked = BLOCK_INDEX_INVALID;
    auto writing_tail_sector = tail_sector();
    auto addr = head_;
    if (writing_tail_sector) {
        linked = fs_->allocator_->allocate(BlockType::File);
        BlockTail tail;
        tail.sector.bytes = buffpos_;
        tail.bytes_in_block = bytes_in_block_;
        tail.linked_block = linked;
        memcpy(tail_info<BlockTail>(buffer_), &tail, sizeof(BlockTail));
    }
    else {
        SectorTail tail;
        tail.bytes = buffpos_;
        memcpy(tail_info<SectorTail>(buffer_), &tail, sizeof(SectorTail));
        head_.add(SectorSize);
    }

    // Write this full sector. No partial writes here because of the tail. Most
    // of the time we write full sectors anyway.
    if (!fs_->storage_->write(addr, buffer_, sizeof(buffer_))) {
        return 0;
    }

    // We could do this in the if scope above, I like doing things "in order" though.
    if (writing_tail_sector) {
        head_ = fs_->initialize_block(linked, id_, head_.block);
        if (!head_.valid()) {
            assert(false); // TODO: Yikes.
        }

        // Every N blocks we save our offset in the tree. This affects how much
        // seeking needs to happen when trying to append or seek around.
        blocks_since_save_++;
        if (blocks_since_save_ == POSITION_SAVE_FREQUENCY) {
            TreeContext<FileSystem::NodeType> tc{ *fs_ };
            auto key = INodeKey::file_position(id_, length_);
            tc.add(key, head_);
            blocks_since_save_ = 0;
        }

        bytes_in_block_ = 0;
    }

    auto flushed = buffpos_;
    buffpos_ = 0;
    return flushed;
}

int32_t OpenFile::read(void *ptr, size_t size) {
    // Are we out of data to return?
    if (available_ == buffpos_) {
        buffpos_ = 0;

        if (!fs_->storage_->read(head_, buffer_, sizeof(buffer_))) {
            return 0;
        }

        // See how much data we have in this sector and/or if we have a block we
        // should be moving onto after this sector is read.
        if (tail_sector()) {
            BlockTail tail;
            memcpy(&tail, tail_info<BlockTail>(buffer_), sizeof(BlockTail));
            available_ = tail.sector.bytes;
            if (tail.linked_block != BLOCK_INDEX_INVALID) {
                head_ = BlockAddress{ tail.linked_block, SectorSize };
            }
            else {
                assert(false);
            }
        }
        else {
            SectorTail tail;
            memcpy(&tail, tail_info<SectorTail>(buffer_), sizeof(SectorTail));
            available_ = tail.bytes;
            head_.add(SectorSize);
        }

        // End of the file? Marked by a "unwritten" sector.
        if (available_ == 0 || available_ == SECTOR_INDEX_INVALID) {
            // If we're at the end we know our length.
            if (length_ == INVALID_LENGTH) {
                length_ = position_;
            }
            available_ = 0;
            return 0;
        }
    }

    auto remaining = (uint16_t)(available_ - buffpos_);
    auto copying = remaining > size ? size : remaining;
    memcpy(ptr, buffer_ + buffpos_, copying);

    buffpos_ += copying;
    position_ += copying;

    return copying;
}

void OpenFile::close() {
    flush();
}

}
