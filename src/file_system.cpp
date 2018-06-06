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
    void add(INodeKey key, uint64_t value) {
        new_head = tree.add(key, value);
    }

    uint64_t find(INodeKey key) {
        return tree.find(key);
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

    tree_addr_ = find_tree();

    return tree_addr_.valid();
}

bool FileSystem::exists(const char *name) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = make_key(2, id);

    return tc.find(key) != 0;
}

OpenFile FileSystem::open(const char *name, bool readonly) {
    TreeContext<NodeType> tc{ *this };

    auto id = crc32_checksum((uint8_t *)name, strlen(name));
    auto key = make_key(2, id);

    auto head = BlockAddress{ };
    auto existing = tc.find(key);
    if (existing == 0) {
        head = initialize_block(allocator_.allocate(), id);
        tc.add(key, head.to_uint64());
    }
    else {
        // TODO: Find last block if writing.
        head = BlockAddress::from_uint64(existing);
        sdebug << "Open: " << head << std::endl;
    }

    return { *this, id, head, readonly };
}

bool FileSystem::close() {
    return storage_->close();
}

bool FileSystem::touch() {
    TreeContext<NodeType> tc{ *this };
    tc.touch();
    return true;
}

// TODO: Move this into the nodes_ implementation.
BlockAddress FileSystem::find_tree() {
    TreeContext<NodeType> tc{ *this };

    auto tree_block = sbm_.tree_block();
    assert(tree_block != BLOCK_INDEX_INVALID);

    auto &geometry = storage_->geometry();
    auto required = tc.serializer.size(true);
    auto iter = BlockAddress{ tree_block, 0 };
    auto found = BlockAddress{ };

    // We could compare TreeHead timestamps, though we always append.
    while (iter.remaining_in_block(geometry) > required) {
        TreeHead head;
        NodeType node;

        if (iter.beginning_of_block()) {
            TreeBlockHeader header;
            if (!storage_->read(iter, &header, sizeof(TreeBlockHeader))) {
                return { };
            }

            if (!header.valid()) {
                return found;
            }

            iter.add(SectorSize);
        }
        else {
            if (nodes_.deserialize(iter, &node, &head)) {
                found = iter;
                iter.add(required);
            }
            else {
                break;
            }
        }
    }

    return found;
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

OpenFile::OpenFile(FileSystem &fs, file_id_t id, BlockAddress head, bool readonly) :
    fs_(&fs), id_(id), head_(head), readonly_(readonly) {
}

size_t OpenFile::write(const void *ptr, size_t size) {
    auto to_write = size;
    auto wrote = 0;

    assert(!readonly_);

    while (to_write > 0) {
        auto remaining = sizeof(buffer_) - sizeof(SectorTail) - position_;
        auto copying = to_write > remaining ? remaining : to_write;

        if (remaining == 0) {
            if (flush() == 0) {
                return wrote;
            }
            /*
            // TODO: Link this block to the previous one.
            head_ = fs_->initialize_block(fs_->allocator_.allocate(), id_);
            if (!head_.valid()) {
                return wrote;
            }
            */
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

size_t OpenFile::read(void *ptr, size_t size) {

    sdebug << "Read: " << head_ << std::endl;

    if (available_ == position_) {
        if (!fs_->storage_->read(head_, buffer_, sizeof(buffer_))) {
            return 0;
        }

        auto tail_offset = sizeof(buffer_) - sizeof(SectorTail);
        auto tail = reinterpret_cast<SectorTail*>(buffer_ + tail_offset);

        if (tail->bytes == 0 || tail->bytes == SECTOR_INDEX_INVALID) {
            return 0;
        }

        available_ = tail->bytes;

        sdebug << "Tail: " << available_ << std::endl;
    }

    auto copying = available_ > size ? size : available_;
    memcpy(ptr, buffer_ + position_, copying);

    position_ += copying;

    return copying;
}

size_t OpenFile::flush() {
    if (readonly_) {
        return 0;
    }
    if (position_ == 0) {
        return 0;
    }

    auto tail_offset = sizeof(buffer_) - sizeof(SectorTail);
    auto tail = reinterpret_cast<SectorTail*>(buffer_ + tail_offset);

    tail->bytes = position_;

    sdebug << "Write: " << head_ << std::endl;

    if (!fs_->storage_->write(head_, buffer_, sizeof(buffer_))) {
        return 0;
    }

    head_.add(position_);

    position_ = 0;

    return 0;
}

void OpenFile::close() {
    flush();
}

}
