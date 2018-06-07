#include <gtest/gtest.h>

#include <confs/block_alloc.h>
#include <confs/inodes.h>
#include <confs/persisted_tree.h>
#include <confs/in_memory_nodes.h>
#include <backends/linux-memory/linux-memory.h>

#include "utilities.h"

using namespace confs;

class PersistedTreeSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
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
    using NodeType = Node<int32_t, int32_t, BlockAddress, 6, 6>;
    auto storage = InMemoryNodeStorage<NodeType>{ 2048 };
    auto cache = MemoryConstrainedNodeCache<NodeType, 8>{ storage };
    auto tree = PersistedTree<NodeType>{ cache };

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
    using NodeType = Node<int32_t, int32_t, BlockAddress, 6, 6>;
    auto storage = InMemoryNodeStorage<NodeType>{ 2048 };
    auto cache = MemoryConstrainedNodeCache<NodeType, 8>{ storage };
    auto tree = PersistedTree<NodeType>{ cache };

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
    using NodeType = Node<int32_t, int32_t, BlockAddress, 6, 6>;
    auto storage = InMemoryNodeStorage<NodeType>{ 1024 * 1024 };
    auto cache = MemoryConstrainedNodeCache<NodeType, 8>{ storage };
    auto tree = PersistedTree<NodeType>{ cache };
    auto map = std::map<NodeType::KeyType, NodeType::ValueType>{};

    srand(1);

    auto value = 1;
    for (auto i = 0; i < 1024; ++i) {
        auto key = random() % UINT32_MAX;
        tree.add(key, value);
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

    using NodeType = Node<uint64_t, int32_t, BlockAddress, 6, 6>;
    auto storage = InMemoryNodeStorage<NodeType>{ 1024 * 1024 };
    auto cache = MemoryConstrainedNodeCache<NodeType, 8>{ storage };
    auto tree = PersistedTree<NodeType>{ cache };
    auto map = std::map<NodeType::KeyType, NodeType::ValueType>{};

    for (auto i = 0; i < 8; ++i) {
        auto inode = (uint32_t)(random() % 2048 + 1024);
        inodes.push_back(inode);

        auto offset = 512;

        for (auto j = 0; j < 128; ++j) {
            auto key = INodeKey(inode, offset);
            tree.add(key, inode);
            map[key] = inode;
            offset += random() % 4096;
        }
    }

    for (auto pair : map) {
        ASSERT_EQ(tree.find(pair.first), pair.second);
    }
}

TEST_F(PersistedTreeSuite, FindLastLessThanLookup) {
    std::vector<uint32_t> inodes;
    std::map<uint32_t, uint32_t> last_offsets;

    using NodeType = Node<uint64_t, int32_t, BlockAddress, 6, 6>;
    auto storage = InMemoryNodeStorage<NodeType>{ 1024 * 1024 };
    auto cache = MemoryConstrainedNodeCache<NodeType, 8>{ storage };
    auto tree = PersistedTree<NodeType>{ cache };
    auto map = std::map<NodeType::KeyType, NodeType::ValueType>{};

    for (auto i = 0; i < 8; ++i) {
        auto inode = (uint32_t)(random() % 2048 + 1024);
        inodes.push_back(inode);

        auto offset = 512;

        for (auto j = 0; j < 128; ++j) {
            auto key = INodeKey(inode, offset);
            tree.add(key, inode);
            last_offsets[inode] = offset;
            map[key] = inode;
            offset += random() % 4096;
        }
    }

    for (auto inode : inodes) {
        NodeType::KeyType found;
        NodeType::ValueType value;

        auto key = INodeKey(inode, UINT32_MAX);

        ASSERT_TRUE(tree.find_last_less_then(key, &value, &found));

        auto key_offset = (found) & ((uint32_t)-1);

        EXPECT_EQ(last_offsets[inode], key_offset);
    }
}
