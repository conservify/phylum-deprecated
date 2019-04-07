#include <gtest/gtest.h>
#include <cstring>

#include "phylum/phylum.h"
#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"
#include "utilities.h"
#include "lorem.h"

using block_index_t = uint32_t;
using block_offset_t = uint32_t;

using namespace phylum;

class BlockOperations;

constexpr block_index_t INVALID_BLOCK_INDEX = (block_index_t)-1;
constexpr block_offset_t INVALID_BLOCK_OFFSET = (block_offset_t)-1;
constexpr file_id_t INVALID_FILE_ID = (file_id_t)-1;

struct BlockTail2 {
    BlockMagic magic;

    /**
     * Total bytes written to this block minus sizeof(BlockHead) and sizeof(BlockTail)
     */
    uint32_t size;

    /**
     * Next block of this kind. Index block or a data block in the same file.
     */
    block_index_t linked_block;

    BlockTail2() {
    }

    BlockTail2(uint32_t size, block_index_t linked_block)
        : magic(BlockMagic::get_valid()), size(size), linked_block(linked_block) {
    }

    bool valid() const {
        return magic.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const BlockTail2 &b) {
    return os << "BlockTail<" << "size=" << b.size << " linked=" << b.linked_block << ">";
}

struct ChunkHead {
    uint32_t sequence;
    file_id_t file_id;
    uint16_t size;
};

struct BlockHead2 {
    BlockMagic magic;
    BlockType type;
    uint32_t sequence;
    file_id_t file_id;

    /**
     *
     */
    BlockAddress indexed;

    /**
     * See "Striping" section in the README
     */
    union {
        struct {
            uint8_t hour;
        } hourly;
        struct {
            uint8_t day;
        } daily;
    } strategy;

    BlockHead2() {
    }

    BlockHead2(BlockType type, uint32_t sequence, file_id_t file_id, BlockAddress indexed)
        : magic(BlockMagic::get_valid()), type(type), sequence(sequence), file_id(file_id), indexed(indexed) {
    }

    bool valid() const {
        return magic.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const BlockHead2 &b) {
    switch (b.type) {
    case BlockType::File: {
        return os << "BlockHead<" << b.sequence << " " << b.type << " file_id=" << b.file_id << " indexed=" << b.indexed << ">";
        break;
    }
    case BlockType::Index: {
        return os << "BlockHead<" << b.sequence << " " << b.type << ">";
        break;
    }
    default: {
        return os << "BlockHead<" << b.sequence << " " << b.type << ">";
        break;
    }
    }
}


constexpr size_t HashSize = 32;

class BlockOperations;

class Allocator {
private:
    block_index_t first_{ 0 };
    block_index_t head_{ 0 };
    block_index_t number_of_blocks_{ 0 };

public:
    bool initialize(Geometry geometry) {
        first_ = geometry.first();
        head_ = geometry.first();
        number_of_blocks_ = geometry.number_of_blocks;
        return true;
    }

    uint32_t available_blocks() const {
        if (head_ < first_) {
            return number_of_blocks_ - (head_ + first_);
        }
        return number_of_blocks_ - (head_ - first_);
    }

    struct Allocation {
        block_index_t block;
        BlockAddress tail;
    };

    Allocation allocate(BlockOperations &bo, BlockType type, file_id_t file_id, BlockAddress indexed);

};

struct TreeNode {
    uint32_t sequence;
    file_id_t file_id;
    BlockAddress addr;
    uint16_t size;
    uint8_t flags;
    uint8_t hash[HashSize];

    bool valid() const {
        return addr.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const TreeNode &node) {
    return os << "Node<" << node.sequence << " file=" << node.file_id << " addr=" << node.addr << " size=" << node.size << ">";
}

struct WriteOperation {
    BlockAddress wrote;
    BlockAddress tail;
    uint32_t block;
    uint32_t chunk;
    bool allocated;

    bool valid() {
        return wrote.valid() && tail.valid();
    }
};

class BlockOperations {
private:
    static constexpr uint8_t Logging = 1;

private:
    uint32_t blocks_{ 0 };
    uint32_t chunks_{ 0 };
    StorageBackend *sf_{ nullptr };
    Allocator *allocator_{ nullptr };

private:
    BlockOperations() {
    }

    friend class File2;

public:
    BlockOperations(StorageBackend &sf, Allocator &allocator) : sf_(&sf), allocator_(&allocator) {
    }

public:
    bool erase(block_index_t block) {
        return sf_->erase(block);
    }

    bool read_block_head(block_index_t block, BlockHead2 &head) {
        return sf_->read(BlockAddress{ block, 0 }, &head, sizeof(BlockHead2));
    }

    WriteOperation write_block_head(BlockAddress addr, BlockType type, file_id_t file_id, BlockAddress indexed) {
        BlockHead2 head{
            type,
            blocks_++,
            file_id,
            indexed
        };

        if (Logging > 0) {
            sdebug() << "BlockOps::write_block_head: addr=" << addr << " " << head << endl;
        }

        assert(addr.position == 0);

        if (!sf_->write(addr, &head, sizeof(BlockHead2))) {
            return WriteOperation{ };
        }

        return WriteOperation{
            addr,
            addr.advance(sizeof(BlockHead2)),
            head.sequence,
            INVALID_SEQUENCE_NUMBER,
            false
        };
    }

    bool read_block_tail(block_index_t block, BlockTail2 &tail) {
        return sf_->read(geometry().block_tail_address_from(BlockAddress{ block, 0 }, sizeof(BlockTail2)), &tail, sizeof(BlockTail2));
    }

    WriteOperation write_block_tail(BlockAddress addr, block_index_t linked_block) {
        BlockTail2 tail{
            addr.position - sizeof(BlockHead2),
            linked_block
        };

        addr = geometry().block_tail_address_from(addr, sizeof(BlockTail2));

        if (Logging > 0) {
            sdebug() << "BlockOps::write_block_tail: addr=" << addr << " " << tail << " linked=" << linked_block << endl;
        }

        if (!sf_->write(addr, &tail, sizeof(BlockTail2))) {
            return WriteOperation{ };
        }

        return WriteOperation{
            addr,
            addr.advance(sizeof(BlockTail2)),
            INVALID_SEQUENCE_NUMBER,
            INVALID_SEQUENCE_NUMBER,
            false
        };
    }

public:
    WriteOperation write_data(BlockAddress addr, file_id_t file_id, BlockAddress indexed, void *data, size_t sz) {
        if (Logging > 2) {
            sdebug() << "BlockOps::write_data_chunk: addr=" << addr << " size=" << sz << endl;
        }

        auto required = sizeof(ChunkHead) + sz;
        auto block_allocated = false;

        // Is there room in this block?
        if (geometry().remaining_in_block(addr, sizeof(BlockTail2)) < required) {
            auto allocated = allocator_->allocate(*this, BlockType::File, file_id, indexed);

            assert(write_block_tail(addr, allocated.block).valid());

            addr = allocated.tail;
            block_allocated = true;
        }

        ChunkHead head{
            chunks_++,
            file_id,
            (uint16_t)sz
        };

        if (!sf_->write(addr, &head, sizeof(ChunkHead))) {
            return WriteOperation{ };
        }

        if (!sf_->write(addr.advance(sizeof(ChunkHead)), data, sz)) {
            return WriteOperation{ };
        }

        return WriteOperation{
            addr,
            addr.advance(sizeof(ChunkHead) + sz),
            INVALID_SEQUENCE_NUMBER,
            head.sequence,
            block_allocated
        };
    }

    bool read_tree_node(BlockAddress addr, TreeNode &node) {
        return sf_->read(addr, &node, sizeof(TreeNode));
    }

    WriteOperation write_tree_node(BlockAddress addr, file_id_t file_id, uint32_t chunk, BlockAddress chunk_addr, size_t sz) {
        if (Logging > 2) {
            sdebug() << "BlockOps::write_tree_node:  addr=" << addr << " size=" << sz << " chunk=" << chunk_addr << endl;
        }

        auto required = sizeof(TreeNode);
        auto block_allocated = false;

        // Is there room in this block?
        if (geometry().remaining_in_block(addr, sizeof(BlockTail2)) < required) {
            auto allocated = allocator_->allocate(*this, BlockType::Index, INVALID_FILE_ID, BlockAddress{ });

            assert(write_block_tail(addr, allocated.block).valid());

            addr = allocated.tail;
            block_allocated = true;
        }

        TreeNode node{
            chunk,
            file_id,
            chunk_addr,
            (uint16_t)sz,
            0,
            { 0 }
        };

        if (!sf_->write(addr, &node, sizeof(TreeNode))) {
            return WriteOperation{ };
        }

        return WriteOperation{
            addr,
            addr.advance(sizeof(TreeNode)),
            INVALID_SEQUENCE_NUMBER,
            node.sequence,
            block_allocated
        };
    }

    Geometry &geometry() const {
        return sf_->geometry();
    }
};

class Tree {
private:
    StorageBackend *sf_{ nullptr };
    Allocator *allocator_{ nullptr };
    BlockOperations *bo_{ nullptr };
    BlockAddress beginning_;
    BlockAddress head_;

public:
    Tree(StorageBackend &sf, Allocator &allocator, BlockOperations &bo) : sf_(&sf), allocator_(&allocator), bo_(&bo) {
    }

public:
    BlockAddress beginning() const {
        return beginning_;
    }

    BlockAddress head() const {
        return head_;
    }

    bool open() {
        auto allocated = allocator_->allocate(*bo_, BlockType::Index, INVALID_FILE_ID, BlockAddress{ });

        beginning_ = allocated.tail;
        head_ = allocated.tail;

        return true;
    }

    WriteOperation append(file_id_t file_id, uint32_t chunk, BlockAddress chunk_addr, size_t sz) {
        auto op = bo_->write_tree_node(head_, file_id, chunk, chunk_addr, sz);
        if (!op.valid()) {
            return WriteOperation{ };
        }

        head_ = op.tail;

        return op;
    }

    TreeNode get(uint32_t sequence) {
        auto g = sf_->geometry();

        auto iterator = beginning_;
        auto visited = (uint32_t)0;

        sdebug() << "Tree::get(" << sequence << ") beginning=" << beginning_ << endl;

        for (auto b = (block_index_t)0; b < g.number_of_blocks; ++b) {
            BlockHead2 head;
            assert(bo_->read_block_head(iterator.block, head));
            assert(head.type == BlockType::Index);

            sdebug() << "Tree::get(" << sequence << ") read_tail=" << iterator << " visited=" << visited << endl;

            BlockTail2 tail;
            assert(bo_->read_block_tail(iterator.block, tail));

            if (tail.valid()) {
                auto nodes = tail.size / sizeof(TreeNode);

                sdebug() << "Tree::get(" << sequence << ") size=" << tail.size << " nodes=" << nodes << endl;

                if (sequence >= (nodes + visited)) {
                    visited += nodes;
                    iterator = BlockAddress{ tail.linked_block, 0 };
                    continue;
                }
            }

            auto position = (sequence - visited) * sizeof(TreeNode) + (sizeof(BlockHead2));

            auto addr = BlockAddress{
                iterator.block,
                position
            };

            sdebug() << "Tree::get(" << sequence << ") read=" << addr << " visited=" << visited << " position=" << position << endl;

            TreeNode node;

            assert(bo_->read_tree_node(addr, node));

            if (node.valid()) {
                return node;
            }

            break;
        }

        return { };
    }

};

struct FileWrite {
    uint32_t chunk;
    bool success;

    FileWrite(uint32_t chunk) : chunk(chunk), success(true) {
    }

    FileWrite(bool success) : success(success) {
    }
};

class File2 {
private:
    BlockOperations *bo_{ nullptr };
    Tree *tree_{ nullptr };
    file_id_t id_;
    BlockAddress head_;

private:
    File2() {
    }

    File2(BlockOperations &bo, Tree &tree, file_id_t id, BlockAddress head) : bo_(&bo), tree_(&tree), id_(id), head_(head) {
    }

    friend class FileSystem2;

public:
    bool valid() {
        return head_.valid();
    }

     FileWrite write(void *data, size_t sz) {
        if (false) {
            sdebug() << "Writing: file=" << id_ << " head=" << head_ << " size=" << sz << endl;
        }

        auto dop = bo_->write_data(head_, id_, tree_->head(), data, sz);
        if (!dop.valid()) {
            return false;
        }

        head_ = dop.tail;

        if (dop.allocated) {
            auto top = tree_->append(id_, dop.chunk, dop.wrote, sz);
            if (!top.valid()) {
                return false;
            }
        }

        return dop.chunk;
    }

};

class Scanner {
private:
    StorageBackend *sf_{ nullptr };
    BlockOperations *bo_{ nullptr };

public:
    Scanner(StorageBackend &sf, BlockOperations &bo) : sf_(&sf), bo_(&bo) {
    }

public:
    void scan() {
        auto g = sf_->geometry();

        sdebug() << "Scanning: " << g << endl;

        for (auto b = g.first(); b < g.first() + g.number_of_blocks; ++b) {
            BlockHead2 head;
            assert(bo_->read_block_head(b, head));

            if (head.valid()) {
                BlockTail2 tail;
                assert(bo_->read_block_tail(b, tail));

                if (tail.valid()) {
                    sdebug() << "Block " << b << " " << head << " " << tail << endl;
                }
                else {
                    sdebug() << "Block " << b << " " << head << endl;
                }

                switch (head.type) {
                case BlockType::Index: {
                    break;
                }
                case BlockType::File: {
                    break;
                }
                default: {
                    break;
                }
                }
            }
        }
    }

    block_index_t find_oldest_block(BlockType type) {
        auto g = sf_->geometry();
        auto block = INVALID_BLOCK_INDEX;
        auto seq = INVALID_SEQUENCE_NUMBER;

        for (auto b = g.first(); b < g.first() + g.number_of_blocks; ++b) {
            BlockHead2 head;
            assert(bo_->read_block_head(b, head));

            if (head.valid()) {
                if (head.type == type) {
                    if (seq == INVALID_SEQUENCE_NUMBER || head.sequence < seq) {
                        block = b;
                        seq = head.sequence;
                    }
                }
            }
        }


        return block;
    }

};

class FileSystem2 {
private:
    StorageBackend *sf_;
    Allocator allocator_;
    BlockOperations bo_;
    Tree tree_;
    File2 files_[2];

public:
    FileSystem2(StorageBackend &sf) : sf_(&sf), allocator_(), bo_(sf, allocator_), tree_(sf, allocator_, bo_) {
    }

public:
    bool erase() {
        return true;
    }

    bool open() {
        assert(allocator_.initialize(geometry()));

        if (!tree_.open()) {
            return false;
        }

        return true;
    }

    File2 open_file(file_id_t id) {
        auto file = files_[(uint8_t)id];
        if (!file.valid()) {
            auto allocated = allocator_.allocate(bo_, BlockType::File, id, tree_.head());

            file = File2{
                bo_,
                tree_,
                id,
                allocated.tail
            };
        }
        return file;
    }

    Geometry &geometry() {
        return sf_->geometry();
    }

    void geometry(Geometry &g) {
        sf_->geometry(g);
    }

    Allocator &allocator() {
        return allocator_;
    }

    Tree &tree() {
        return tree_;
    }

    Scanner scanner() {
        return Scanner{ *sf_, bo_ };
    }

};

Allocator::Allocation Allocator::allocate(BlockOperations &bo, BlockType type, file_id_t file_id, BlockAddress indexed) {
    assert(available_blocks() > 0);

    auto new_block = head_++;

    // Handle wrap around.
    auto &g = bo.geometry();
    if (new_block == g.number_of_blocks) {
        new_block = g.first();
    }

    if (true) {
        sdebug() << "Allocated: " << new_block << " " << type << endl;
    }

    assert(bo.erase(new_block));

    auto head_op = bo.write_block_head(BlockAddress{ new_block, 0 }, type, file_id, indexed);
    assert(head_op.valid());

    return {
        new_block,
        BlockAddress{ new_block, sizeof(BlockHead2) }
    };
}

class Compactor {
private:
    FileSystem2 *fs_;

public:
    Compactor(FileSystem2 &fs) : fs_(&fs) {
    }

public:
    bool run();

};

bool Compactor::run() {
    // Drop the oldest Data block.... which, should be the first data block after the head.
    auto scanner = fs_->scanner();
    auto found = scanner.find_oldest_block(BlockType::File);

    sdebug() << "Oldest Block: " << found << endl;

    // What happens to the tree? Can we mark that block as being recycled so
    // when we rewrite the tree in the future we can update the flags?

    // Also, there's no reason keeping index around for data we got rid of.

    // memory.geometry(memory.geometry().fake(64, 6));

    return true;
}

class Rev2Suite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_;
    FileSystem2 fs_{ storage_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.open());
    }

    void TearDown() override {
    }

};

TEST_F(Rev2Suite, Example) {
    auto logs = fs_.open_file((file_id_t)0);

    for (auto i = 0; i < 4096; ++i) {
        char message[64];
        LoremIpsum lorem;
        lorem.sentence(message, sizeof(message));

        auto wrote = logs.write(message, strlen(message));
        assert(wrote.success);

        if (fs_.allocator().available_blocks() == 2) {
            Compactor compactor(fs_);
            auto done = compactor.run();
            assert(done);
            assert(fs_.allocator().available_blocks() > 2);
        }
    }

    auto scanner = fs_.scanner();

    scanner.scan();
}
