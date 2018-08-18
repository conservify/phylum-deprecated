#include "phylum/file_system.h"
#include "phylum/stack_node_cache.h"

namespace phylum {

// NOTE: This is typically ~1700 bytes with 8 entries. With the union between
// children and values in NodeType this weighs in around 1300 bytes.
template<typename NodeType>
struct TreeContext {
public:
    FileSystem &fs;
    NodeSerializer<NodeType> serializer;
    MemoryConstrainedNodeCache<NodeType, 12> cache;
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

    bool recreate() {
        auto before = fs.nodes_.state();

        new_head = tree.recreate();
        if (!new_head.valid()) {
            return false;
        }

        auto after = fs.nodes_.state();

        if (before.index.valid()) {
            assert(before.index != after.index);
            fs.fpm().free(before.index.block);
        }

        assert(before.leaf != after.leaf);
        fs.fpm().free(before.leaf.block);

        return true;
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

FileSystem::FileSystem(StorageBackend &storage, BlockManager &allocator) :
    storage_(&storage), allocator_(&allocator), sbm_{ storage, allocator },
    nodes_{ storage, allocator }, journal_(storage, allocator),
    fpm_(storage, allocator) {
}

void FileSystem::prepare(TreeFileSystemSuperBlock &sb) {
    // NOTE: This is done again in SuperBlockManager::save
    auto allocator_state = allocator_->state();
    sb.allocator = allocator_state;

    auto tree_state = nodes_.state();
    sb.index = tree_state.index;
    sb.leaf = tree_state.leaf;
    sb.tree = tree_addr_.block;
}

bool FileSystem::format() {
    if (!sbm_.create()) {
        return false;
    }

    if (!sbm_.locate()) {
        return false;
    }

    auto &sb = sbm_.block();

    if (!journal_.format(sb.journal)) {
        return false;
    }

    if (!fpm_.format(sb.free)) {
        return false;
    }

    return touch();
}

bool FileSystem::mount(bool wipe) {
    allocator_->initialize(storage_->geometry());

    if (wipe || !sbm_.locate()) {
        if (!format()) {
            return false;
        }
    }

    auto &sb = sbm_.block();

    if (!journal_.locate(sb.journal)) {
        return false;
    }

    if (!fpm_.locate(sb.free)) {
        return false;
    }

    tree_addr_ = nodes_.find_head(sb.tree);
    if (!tree_addr_.valid()) {
        return false;
    }

    nodes_.state({ sb.index, sb.leaf });

    return true;
}

bool FileSystem::exists(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto key = INodeKey::file_beginning(name);

    return tc.find(key) != 0;
}

OpenFile FileSystem::open(const char *name, bool readonly) {
    auto id = INodeKey::file_id(name);
    auto file = OpenFile{ *this, id, readonly };

    file.open_or_create();

    return file;
}

bool FileSystem::touch() {
    TreeContext<NodeType> tc{ *this };
    tc.touch();

    return true;
}

bool FileSystem::gc() {
    TreeContext<NodeType> tc{ *this };
    if (!tc.recreate()) {
        return false;
    }

    auto &sb = sbm_.block();
    sb.last_gc = sbm_.timestamp();

    return true;
}

bool FileSystem::unmount() {
    return storage_->close();
}

template<typename T, size_t N>
static T *tail_info(uint8_t(&buffer)[N]) {
    auto tail_offset = sizeof(buffer) - sizeof(T);
    return reinterpret_cast<T*>(buffer + tail_offset);
}

OpenFile::OpenFile(FileSystem &fs, file_id_t id, bool readonly) :
    fs_(&fs), id_(id), readonly_(readonly), length_(readonly ? InvalidLengthOrPosition : 0) {
    assert(sizeof(buffer_) == SectorSize);
}

bool OpenFile::open_or_create() {
    if (open()) {
        return true;
    }

    if (readonly_) {
        TreeContext<FileSystem::NodeType> tc{ *fs_ };

        auto beginning = tc.find(INodeKey::file_beginning(id_));
        if (beginning == 0) {
            return false;
        }

        head_ = BlockAddress::from(beginning);
    }
    else {
        auto seeked = seek(Seek::End, 0);
        if (seeked == SeekFailed) {
            TreeContext<FileSystem::NodeType> tc{ *fs_ };

            auto new_block = initialize_block(fs_->allocator_->allocate(BlockType::File), BLOCK_INDEX_INVALID);
            if (!new_block.valid()) {
                return false;
            }

            tc.add(INodeKey::file_beginning(id_), new_block.value());

            head_ = new_block;
        }
    }

    return true;
}

bool OpenFile::open() {
    return head_.valid();
}

bool OpenFile::tail_sector() {
    return head_.tail_sector(fs_->storage().geometry());
}

uint32_t OpenFile::size() {
    if (length_ == InvalidLengthOrPosition) {
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
    if (!tc.find_less_then(INodeKey::file_position(id_, relative), &value, &saved)) {
        return SeekFailed;
    }

    // Make sure we get a key from our file. This happens when the file hasn't
    // been created yet and we get a key from another file.
    auto key = INodeKey(saved);
    if (key.upper() != id_) {
        return SeekFailed;
    }

    // This gets us pretty close (within PositionSaveFrequency blocks) so we
    // walk the blocks and sectors to find the actual location.
    auto starting = key.lower();
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
        auto overhead = tail_sector() ? sizeof(FileBlockTail) : sizeof(FileSectorTail);
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
    auto alloc = AllocatedBlock{ };
    if (writing_tail_sector) {
        alloc = fs_->allocator_->allocate(BlockType::File);
        linked = alloc.block;
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
    if (!fs_->storage_->write(addr, buffer_, sizeof(buffer_))) {
        return 0;
    }

    // We could do this in the if scope above, I like doing things "in order" though.
    if (writing_tail_sector) {
        head_ = initialize_block(alloc, head_.block);
        if (!head_.valid()) {
            assert(false); // TODO: Yikes.
        }

        // Every N blocks we save our offset in the tree. This affects how much
        // seeking needs to happen when trying to append or seek around.
        blocks_since_save_++;
        if (blocks_since_save_ == PositionSaveFrequency) {
            TreeContext<FileSystem::NodeType> tc{ *fs_ };
            auto key = INodeKey::file_position(id_, length_);
            tc.add(key, head_.value());
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
            FileBlockTail tail;
            memcpy(&tail, tail_info<FileBlockTail>(buffer_), sizeof(FileBlockTail));
            available_ = tail.sector.bytes;
            if (tail.block.linked_block != BLOCK_INDEX_INVALID) {
                head_ = BlockAddress{ tail.block.linked_block, SectorSize };
            }
            else {
                assert(false);
            }
        }
        else {
            FileSectorTail tail;
            memcpy(&tail, tail_info<FileSectorTail>(buffer_), sizeof(FileSectorTail));
            available_ = tail.bytes;
            head_.add(SectorSize);
        }

        // End of the file? Marked by a "unwritten" sector.
        if (available_ == 0 || available_ == SECTOR_INDEX_INVALID) {
            // If we're at the end we know our length.
            if (length_ == InvalidLengthOrPosition) {
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

BlockAddress OpenFile::initialize_block(AllocatedBlock alloc, block_index_t previous) {
    FileBlockHead head;

    head.fill();
    head.file_id = id_;
    head.block.linked_block = previous;

    if (!alloc.erased) {
        if (!fs_->storage_->erase(alloc.block)) {
            return { };
        }
    }

    if (!fs_->storage_->write({ alloc.block, 0 }, &head, sizeof(FileBlockHead))) {
        return { };
    }

    return BlockAddress { alloc.block, SectorSize };
}

}
