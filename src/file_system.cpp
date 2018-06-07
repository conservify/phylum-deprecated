#include "confs/file_system.h"

namespace confs {

struct FileBlockHeader {
    BlockAllocSector header;
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

        if (fs.tree_addr_.valid()) {
            tree.head(fs.tree_addr_);
        }
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

    bool find_last_less_then(uint64_t key, uint64_t *value, uint64_t *found) {
        return tree.find_last_less_then(key, value, found);
    }

    void touch() {
        new_head = tree.create_if_necessary();
    }

    bool flush() {
        if (new_head.valid()) {
            fs.sbm_.save(new_head.block);
            fs.tree_addr_ = new_head;
            new_head.invalid();
            return true;
        }
        return true;
    }
};

bool FileSystem::initialize(bool wipe) {
    if (!storage_->open()) {
        return false;
    }

    return open(wipe);
}

bool FileSystem::open(bool wipe) {
    if (wipe || !sbm_.locate()) {
        if (!format()) {
            return false;
        }
    }

    tree_addr_ = nodes_.find_head(sbm_.tree_block());

    return tree_addr_.valid();
}

bool FileSystem::exists(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = INodeKey::file_beginning(id);

    return tc.find(key) != 0;
}

OpenFile FileSystem::open(const char *name, bool readonly) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = INodeKey::file_beginning(id);

    auto existing = tc.find(key);
    if (existing == 0) {
        auto head = initialize_block(allocator_.allocate(), id);
        tc.add(key, head.to_uint64());
        return { *this, id, head, readonly };
    }

    // TODO: Find a better starting address for the seek.
    auto head = BlockAddress::from_uint64(existing);
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

BlockAddress FileSystem::initialize_block(block_index_t block, file_id_t file_id) {
    FileBlockHeader header;

    header.fill();
    header.file_id = file_id;

    if (!storage_->erase(block)) {
        return { };
    }

    if (!storage_->write({ block, 0 }, 0, &header, sizeof(FileBlockHeader))) {
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
    fs_(&fs), id_(id), head_(head), readonly_(readonly) {
    assert(sizeof(buffer_) == SectorSize);
}

bool OpenFile::tail_sector() {
    return head_.tail_sector(fs_->storage().geometry());
}

int32_t OpenFile::seek(Seek seek) {
    TreeContext<FileSystem::NodeType> tc{ *fs_ };

    switch (seek) {
    case Seek::End: {
        uint64_t value;
        uint64_t saved;
        if (tc.find_last_less_then(INodeKey::file_maximum(id_), &value, &saved)) {
            head_ = BlockAddress::from_uint64(value);
            length_ = INodeKey(saved).lower();
        }
        break;
    }
    case Seek::Beginning: {
        head_ = BlockAddress::from_uint64(tc.find(INodeKey::file_beginning(id_)));
        return 0;
    }
    }

    blocks_since_save_ = 0;

    auto &g = fs_->storage().geometry();
    auto starting = head_;
    auto addr = BlockAddress::tail_sector_of(starting.block, g);
    while (true) {
        if (!fs_->storage_->read(addr, buffer_, sizeof(buffer_))) {
            return { };
        }

        if (addr.tail_sector(g)) {
            auto tail = tail_info<BlockTail>(buffer_);
            if (tail->linked_block != BLOCK_INDEX_INVALID) {
                blocks_since_save_++;
                addr = BlockAddress::tail_sector_of(tail->linked_block, g);
            }
            else {
                addr = BlockAddress{ addr.block, SectorSize };
            }
        }
        else {
            auto tail = tail_info<SectorTail>(buffer_);
            if (tail->bytes == 0 || tail->bytes == SECTOR_INDEX_INVALID) {
                head_ = addr;
                break;
            }
            addr.add(SectorSize);
        }
    }

    return 0;
}

int32_t OpenFile::write(const void *ptr, size_t size) {
    auto to_write = size;
    auto wrote = 0;

    assert(!readonly_);

    while (to_write > 0) {
        auto overhead = tail_sector() ? sizeof(BlockTail) : sizeof(SectorTail);
        auto remaining = sizeof(buffer_) - overhead - position_;
        auto copying = to_write > remaining ? remaining : to_write;

        if (remaining == 0) {
            if (flush(BLOCK_INDEX_INVALID) == 0) {
                return wrote;
            }
        }
        else {
            memcpy(buffer_ + position_, (const uint8_t *)ptr + wrote, copying);
            position_ += copying;
            wrote += copying;
            to_write -= copying;
        }
    }

    return wrote;
}

int32_t OpenFile::flush(block_index_t linked) {
    if (readonly_) {
        return 0;
    }

    if (position_ == 0) {
        return 0;
    }

    auto addr = head_;
    if (tail_sector()) {
        auto tail = tail_info<BlockTail>(buffer_);
        linked = fs_->allocator_.allocate();
        tail->sector.bytes = position_;
        tail->linked_block = linked;
        head_ = fs_->initialize_block(linked, id_);
        if (!head_.valid()) {
            assert(false); // TODO: Yikes.
        }

        blocks_since_save_++;
        if (blocks_since_save_ == 8) {
            TreeContext<FileSystem::NodeType> tc{ *fs_ };
            auto key = INodeKey::file_position(id_, length_ + position_);
            tc.add(key, head_.to_uint64());
            blocks_since_save_ = 0;
        }
    }
    else {
        auto tail = tail_info<SectorTail>(buffer_);
        tail->bytes = position_;
        head_.add(SectorSize);
    }

    if (!fs_->storage_->write(addr, buffer_, sizeof(buffer_))) {
        return 0;
    }

    auto flushed = position_;
    length_ += position_;
    position_ = 0;
    return flushed;
}

int32_t OpenFile::read(void *ptr, size_t size) {
    if (available_ == position_) {
        position_ = 0;

        if (!fs_->storage_->read(head_, buffer_, sizeof(buffer_))) {
            return 0;
        }

        if (tail_sector()) {
            auto tail = tail_info<BlockTail>(buffer_);
            if (tail->linked_block != BLOCK_INDEX_INVALID) {
                head_ = BlockAddress{ tail->linked_block, SectorSize };
            }
            else {
                assert(false);
            }
            available_ = tail->sector.bytes;
        }
        else {
            auto tail = tail_info<SectorTail>(buffer_);
            available_ = tail->bytes;
            head_.add(SectorSize);
        }

        if (available_ == 0 || available_ == SECTOR_INDEX_INVALID) {
            available_ = 0;
            return 0;
        }
    }

    auto remaining = available_ - position_;
    auto copying = remaining > size ? size : remaining;
    memcpy(ptr, buffer_ + position_, copying);

    position_ += copying;

    return copying;
}

void OpenFile::close() {
    flush(BLOCK_INDEX_INVALID);
}

}
