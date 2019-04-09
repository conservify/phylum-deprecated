#include <gtest/gtest.h>
#include <cstring>

#include "phylum/phylum.h"
#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"
#include "utilities.h"
#include "lorem.h"

namespace phylum {

using block_index_t = uint32_t;
using block_offset_t = uint32_t;
using SequenceNumber = uint32_t;

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
    SequenceNumber sequence;
    file_id_t file_id;
    uint16_t size;
};

struct BlockHead2 {
    BlockMagic magic;
    BlockType type;
    SequenceNumber sequence;
    file_id_t file_id;

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

    BlockHead2(BlockType type, SequenceNumber sequence, file_id_t file_id)
        : magic(BlockMagic::get_valid()), type(type), sequence(sequence), file_id(file_id) {
    }

    bool valid() const {
        return magic.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const BlockHead2 &b) {
    switch (b.type) {
    case BlockType::File: {
        return os << "BlockHead<" << b.sequence << " " << b.type << " file_id=" << b.file_id << ">";
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
    StorageBackend *sb_{ nullptr };
    block_index_t first_{ 0 };
    block_index_t head_{ 0 };
    block_index_t number_of_blocks_{ 0 };
    block_index_t available_blocks_{ 0 };
    uint8_t *map_{ nullptr };

public:
    Allocator(StorageBackend &sb): sb_(&sb) {
    }

    virtual ~Allocator() {
        if (map_ != nullptr) {
            ::free(map_);
            map_ = nullptr;
        }
    }

public:
    bool initialize(Geometry geometry) {
        first_ = geometry.first();
        head_ = geometry.first();
        number_of_blocks_ = geometry.number_of_blocks;
        available_blocks_ = number_of_blocks_;
        map_ = (uint8_t *)malloc(number_of_blocks_ / 8);

        for (auto b = (block_index_t)0; b < number_of_blocks_; ++b) {
            assert(sb_->erase(b));
        }

        return true;
    }

    block_index_t head() {
        return head_;
    }

    uint32_t available_blocks() const {
        return available_blocks_;
    }

    struct Allocation {
        block_index_t block;
        BlockAddress tail;

        BlockAddress head() {
            return { block, sizeof(BlockHead2) };
        }

        bool valid() {
            return block != INVALID_BLOCK_INDEX;
        }
    };

    Allocation allocate(BlockOperations &bo, BlockType type, file_id_t file_id);

    bool free(block_index_t block);

};

struct TreeNode {
    SequenceNumber sequence;
    SequenceNumber version;
    file_id_t file_id;
    BlockAddress addr;
    uint16_t size;
    uint8_t flags;
    uint8_t hash[HashSize];

    bool valid() const {
        return addr.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const TreeNode &n) {
    return os << "Node<v" << n.version << " " << n.sequence << " file=" << n.file_id << " addr=" << n.addr << " size=" << n.size << ">";
}

struct WriteOperation {
    BlockAddress wrote;
    BlockAddress tail;
    block_index_t block;
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
    SequenceNumber blocks_{ 0 };
    SequenceNumber chunks_{ 0 };
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

    WriteOperation write_block_head(BlockAddress addr, BlockType type, file_id_t file_id) {
        BlockHead2 head{
            type,
            blocks_++,
            file_id
        };

        assert(addr.position == 0);

        if (Logging > 0) {
            sdebug() << "BlockOps::write_block_head: addr=" << addr << " " << head << endl;
        }

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
            addr.position - (uint32_t)sizeof(BlockHead2),
            linked_block
        };

        addr = geometry().block_tail_address_from(addr, sizeof(BlockTail2));

        if (Logging > 0) {
            sdebug() << "BlockOps::write_block_tail: addr=" << addr << " " << tail << endl;
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
    WriteOperation write_data(BlockAddress addr, file_id_t file_id, void *data, size_t sz) {
        if (Logging > 2) {
            sdebug() << "BlockOps::write_data_chunk: addr=" << addr << " size=" << sz << endl;
        }

        auto required = sizeof(ChunkHead) + sz;
        auto block_allocated = false;

        // Is there room in this block?
        if (!addr.valid() || geometry().remaining_in_block(addr, sizeof(BlockTail2)) < required) {
            auto allocated = allocator_->allocate(*this, BlockType::File, file_id);

            if (addr.valid()) {
                assert(write_block_tail(addr, allocated.block).valid());
            }

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

    WriteOperation write_tree_node(BlockAddress addr, TreeNode node) {
        auto required = sizeof(TreeNode);
        auto block_allocated = false;

        // Is there room in this block?
        if (geometry().remaining_in_block(addr, sizeof(BlockTail2)) < required) {
            auto allocated = allocator_->allocate(*this, BlockType::Index, INVALID_FILE_ID);

            assert(write_block_tail(addr, allocated.block).valid());

            addr = allocated.tail;
            block_allocated = true;
        }

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

    WriteOperation write_tree_node(BlockAddress addr, SequenceNumber version, file_id_t file_id, SequenceNumber chunk, BlockAddress chunk_addr, size_t sz) {
        if (Logging > 2) {
            sdebug() << "BlockOps::write_tree_node:  addr=" << addr << " size=" << sz << " chunk=" << chunk_addr << " version=" << version << endl;
        }

        TreeNode node{
            chunk,
            version,
            file_id,
            chunk_addr,
            (uint16_t)sz,
            0,
            { 0 }
        };

        return write_tree_node(addr, node);
    }

    Geometry &geometry() const {
        return sf_->geometry();
    }
};

class Tree {
private:
    SequenceNumber version_{ 0 };
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
        auto allocated = allocator_->allocate(*bo_, BlockType::Index, INVALID_FILE_ID);

        beginning_ = allocated.tail;
        head_ = allocated.tail;

        return true;
    }

    WriteOperation append(file_id_t file_id, uint32_t chunk, BlockAddress chunk_addr, size_t sz) {
        auto op = bo_->write_tree_node(head_, version_, file_id, chunk, chunk_addr, sz);
        if (!op.valid()) {
            return WriteOperation{ };
        }

        head_ = op.tail;

        return op;
    }

    bool copy_nodes(BlockAddress from, BlockAddress to) {
        return true;
    }

    bool rewrite(block_index_t removing) {
        auto g = sf_->geometry();
        auto iterator = beginning_;
        auto destiny = head();

        sdebug() << "Tree::rewrite() beginning=" << beginning_ << endl;

        for (auto b = (block_index_t)0; b < g.number_of_blocks; ++b) {
            BlockHead2 head;
            assert(bo_->read_block_head(iterator.block, head));
            assert(head.type == BlockType::Index);

            BlockTail2 tail;
            assert(bo_->read_block_tail(iterator.block, tail));

            sdebug() << "Tree::rewrite() block: " << iterator.block << endl;

            auto source = BlockAddress{ iterator.block, sizeof(BlockHead2) };

            while (true) {
                TreeNode node;
                auto rop = bo_->read_tree_node(source, node);
                assert(rop);

                if (node.sequence == INVALID_SEQUENCE_NUMBER) {
                    break;
                }

                if (node.addr.block == removing || node.version != version_) {
                    if (node.version > version_) {
                        break;
                    }
                    source = source.advance(sizeof(TreeNode));
                    continue;
                }
                else {
                    sdebug() << "Copy: " << node << " " << source << " -> " << destiny << endl;
                }

                node.version = version_ + 1;

                auto wop = bo_->write_tree_node(destiny, node);
                assert(wop.valid());

                source = source.advance(sizeof(TreeNode));
                destiny = wop.tail;
            }

            if (tail.valid()) {
                iterator = BlockAddress{ tail.linked_block, 0 };
            }
            else {
                break;
            }
        }

        version_++;
        head_ = destiny;

        sdebug() << "Tree::rewrite() done" << endl;

        return true;
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
                (uint32_t)position
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
    bool open() {
        return true;
    }

    bool valid() {
        return head_.valid();
    }

    FileWrite write(void *data, size_t sz) {
        if (false) {
            sdebug() << "Writing: file=" << id_ << " head=" << head_ << " size=" << sz << endl;
        }

        auto dop = bo_->write_data(head_, id_, data, sz);
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

    struct FoundBlock {
        block_index_t block{ INVALID_BLOCK_INDEX };
        SequenceNumber sequence{ INVALID_SEQUENCE_NUMBER };
        BlockType type;

        FoundBlock() {
        }

        FoundBlock(block_index_t block, SequenceNumber sequence, BlockType type) :
            block(block), sequence(sequence), type(type) {
        }

        bool older_than(FoundBlock other) {
            return (sequence == INVALID_SEQUENCE_NUMBER) || (other.sequence == INVALID_SEQUENCE_NUMBER) || (other.sequence > sequence);
        }
    };

    block_index_t block_offset_to_index(block_index_t offset) {
        auto g = sf_->geometry();
        return (offset + g.first()) % g.number_of_blocks;
    }

    FoundBlock first_used_block_after(block_index_t block, BlockType type) {
        auto g = sf_->geometry();

        for (auto offset = (block_index_t)0; offset < g.number_of_blocks; ++offset) {
            auto b = block_offset_to_index(block + offset);

            BlockHead2 head;
            assert(bo_->read_block_head(b, head));

            if (head.valid()) {
                if (head.type == type) {
                    return { b, head.sequence, head.type };
                }
            }
        }

        assert(false);

        return { };
    }

    FoundBlock find_oldest_block(BlockType type) {
        auto g = sf_->geometry();
        auto found = FoundBlock{ };

        for (auto offset = (block_index_t)0; offset < g.number_of_blocks; ++offset) {
            auto b = block_offset_to_index(offset);

            BlockHead2 head;
            assert(bo_->read_block_head(b, head));

            if (head.valid()) {
                if (head.type == type) {
                    auto candidate = FoundBlock{ b, head.sequence, head.type };
                    if (candidate.older_than(found)) {
                        found = candidate;
                    }
                }
            }
        }


        return found;
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
    FileSystem2(StorageBackend &sf) : sf_(&sf), allocator_(sf), bo_(sf, allocator_), tree_(sf, allocator_, bo_) {
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
            file = File2{
                bo_,
                tree_,
                id,
                { }
            };

            assert(file.open());
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

Allocator::Allocation Allocator::allocate(BlockOperations &bo, BlockType type, file_id_t file_id) {
    auto &g = bo.geometry();

    assert(available_blocks() > 0);

    for (auto b = (block_index_t)0; b < g.number_of_blocks; ++b) {
        BlockHead2 head;

        auto block = (b + head_) % g.number_of_blocks;

        assert(bo.read_block_head(block, head));

        if (!head.valid()) {
            head_ = block;
            break;
        }
    }

    available_blocks_--;

    if (true) {
        sdebug() << "Allocated: " << head_ << " " << type << " available=" << available_blocks_ << endl;
    }

    // We're gonna erase as we go, for now.
    // assert(bo.erase(head_));

    auto head_op = bo.write_block_head(BlockAddress{ head_, 0 }, type, file_id);
    assert(head_op.valid());

    return {
        head_,
        BlockAddress{ head_, sizeof(BlockHead2) }
    };
}

bool Allocator::free(block_index_t block) {
    available_blocks_++;

    sdebug() << "Free: " << block << " available=" << available_blocks_ << endl;

    return sb_->erase(block);
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
    auto &allocator = fs_->allocator();
    auto &tree = fs_->tree();
    auto scanner = fs_->scanner();
    auto old = scanner.find_oldest_block(BlockType::File);

    sdebug() << "Candidate: " << old.block << endl;

    assert(tree.rewrite(old.block));

    // TODO Need a way to indicate that the tree was completely written. Something atomic like the superblock we used to have.
    assert(allocator.free(old.block));

    return true;
}

}

using namespace phylum;

class Rev2Suite : public ::testing::Test {
protected:
    Geometry geometry_{ 6, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_;
    FileSystem2 fs_{ storage_ };

protected:
    void SetUp() override {
        storage_.strict_sectors(false);

        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.open());
    }

    void TearDown() override {
    }

};

TEST_F(Rev2Suite, Example) {
    // storage_.log().logging(true);

    auto scanner = fs_.scanner();
    auto logs = fs_.open_file((file_id_t)0);

    // Theoretical maximum index size: (2048blocks/bank * 4banks) * 64bytes = 512k

    for (auto i = 0; i < 512 * 20; ++i) {
        char message[64];
        LoremIpsum lorem;
        lorem.sentence(message, sizeof(message));

        auto wrote = logs.write(message, strlen(message));
        assert(wrote.success);

        if (fs_.allocator().available_blocks() == 1) {
            Compactor compactor(fs_);
            auto done = compactor.run();
            assert(done);
            // assert(fs_.allocator().available_blocks() > 1);

            scanner.scan();
        }
    }

    sdebug() << "Done. Final Scan." << endl;

    scanner.scan();
}
