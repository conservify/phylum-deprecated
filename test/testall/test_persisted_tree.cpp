#include <gtest/gtest.h>

#include "utilities.h"
#include "confs/persisted_tree.h"
#include "confs/block_alloc.h"
#include "backends/linux-memory/linux-memory.h"

using namespace confs;

class PersistedTreeSuite : public ::testing::Test {
protected:
    confs_geometry_t geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    BlockAllocator allocator_{ storage_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

    void TearDown() override {
        ASSERT_TRUE(storage_.close());
    }

};

TEST_F(PersistedTreeSuite, BuildTree) {
    auto memory = InMemoryNodeStorage<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ 2048 };
    auto nodes = MemoryConstrainedNodeCache<int32_t, int32_t, confs_sector_addr_t, 6, 6, 8>{ memory };
    auto tree = PersistedTree<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ nodes };

    tree.add(100, 5738);

    ASSERT_EQ(tree.find(100), 5738);

    tree.add(10, 1);
    tree.add(22, 2);
    tree.add(8, 3);
    tree.add(3, 4);
    tree.add(17, 5);
    tree.add(9, 6);
    tree.add(30, 7);

    ASSERT_EQ(tree.find(30), 7);
    ASSERT_EQ(tree.find(100), 5738);

    tree.add(20, 8);

    ASSERT_EQ(tree.find(20), 8);
    ASSERT_EQ(tree.find(30), 7);
    ASSERT_EQ(tree.find(100), 5738);
}

TEST_F(PersistedTreeSuite, Remove) {
    auto memory = InMemoryNodeStorage<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ 2048 };
    auto nodes = MemoryConstrainedNodeCache<int32_t, int32_t, confs_sector_addr_t, 6, 6, 8>{ memory };
    auto tree = PersistedTree<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ nodes };

    tree.add(100, 5738);

    ASSERT_EQ(tree.find(100), 5738);

    tree.add(10, 1);
    tree.add(22, 2);
    tree.add(8, 3);
    tree.add(3, 4);
    tree.add(17, 5);
    tree.add(9, 6);
    tree.add(30, 7);

    ASSERT_EQ(tree.find(100), 5738);

    ASSERT_TRUE(tree.remove(100));

    ASSERT_EQ(tree.find(100), 0);
}

TEST_F(PersistedTreeSuite, MultipleLookupRandom) {
    auto memory = InMemoryNodeStorage<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ 1024 * 1024 };
    auto nodes = MemoryConstrainedNodeCache<int32_t, int32_t, confs_sector_addr_t, 6, 6, 8>{ memory };
    auto tree = PersistedTree<int32_t, int32_t, confs_sector_addr_t, 6, 6>{ nodes };
    auto map = std::map<int32_t, int32_t>{};

    using StandardTree = BPlusTree<int32_t, int32_t, 6, 6>;
    StandardTree other_tree;

    srand(1);

    auto value = 1;
    for (auto i = 0; i < 1024; ++i) {
        auto key = random() % UINT32_MAX;
        tree.add(key, value);
        other_tree.add(key, value);
        map[key] = value;
        ASSERT_EQ(tree.find(key), value);

        value++;
    }

    for (auto pair : map) {
        ASSERT_EQ(tree.find(pair.first), pair.second);
    }
}

TEST_F(PersistedTreeSuite, MultipleLookupCustomKeyType) {
    std::vector<uint32_t> inodes;

    auto storage = InMemoryNodeStorage<btree_key_t, int32_t, confs_sector_addr_t, 6, 6>{ 1024 * 1024 };
    auto nodes = MemoryConstrainedNodeCache<btree_key_t, int32_t, confs_sector_addr_t, 6, 6, 8>{ storage };
    auto tree = PersistedTree<btree_key_t, int32_t, confs_sector_addr_t, 6, 6>{ nodes };
    auto map = std::map<btree_key_t, int32_t>{};

    for (auto i = 0; i < 8; ++i) {
        auto inode = (uint32_t)(random() % 2048 + 1024);
        inodes.push_back(inode);

        auto offset = 512;

        for (auto j = 0; j < 128; ++j) {
            auto key = make_key(inode, offset);
            tree.add(key, inode);
            map[key] = inode;
            offset += random() % 4096;
        }
    }

    for (auto pair : map) {
        ASSERT_EQ(tree.find(pair.first), pair.second);
    }
}

template<class S>
class Example {
public:
    typename S::KeyType key;

};

TEST_F(PersistedTreeSuite, Schema) {
    using Schema = PersistedTreeSchema<int32_t, int32_t, confs_sector_addr_t, 6, 6>;
    using TreeType = PersistedTree<Schema::KeyType, Schema::ValueType, Schema::AddressType, Schema::Keys, Schema::Values>;
    using NodesType = MemoryConstrainedNodeCache<Schema::KeyType, Schema::ValueType, Schema::AddressType, Schema::Keys, Schema::Values, 8>;

    auto storage = InMemoryNodeStorage<Schema::KeyType, Schema::ValueType, Schema::AddressType, Schema::Keys, Schema::Values>{ 1024 * 1024 };
    NodesType nodes{ storage };
    TreeType tree{ nodes };

    sdebug << "sizeof(NodeType) = " << sizeof(Schema::NodeType) << std::endl;
}

