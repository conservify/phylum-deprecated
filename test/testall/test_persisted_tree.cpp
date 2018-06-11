#include <gtest/gtest.h>

#include "phylum/block_alloc.h"
#include "phylum/inodes.h"
#include "phylum/persisted_tree.h"
#include "phylum/in_memory_nodes.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

struct InMemoryTreeConfiguration {
    using NodeType = Node<uint64_t, int32_t, BlockAddress, 6, 6>;
    using NodeRefType = NodeRef<BlockAddress>;

    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend backend_;
    QueueBlockAllocator allocator_{ geometry_ };
    InMemoryNodeStorage<NodeType> nodes_{ 128 * 1024 };
    MemoryConstrainedNodeCache<NodeType, 8> cache_{ nodes_ };
};

struct StorageBackendTreeConfiguration {
    using NodeType = Node<uint64_t, int32_t, BlockAddress, 6, 6>;
    using NodeRefType = NodeRef<BlockAddress>;

    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend backend_;
    QueueBlockAllocator allocator_{ geometry_ };
    StorageBackendNodeStorage<NodeType> nodes_{ backend_, allocator_ };
    MemoryConstrainedNodeCache<NodeType, 8> cache_{ nodes_ };
};

template<typename T>
class PersistedTreeSuite : public ::testing::Test {
protected:
    // using NodeType = typename T::NodeType;
    // using NodeRefType = typename T::NodeRefType;

    T cfg_;

protected:
    void SetUp() override {
        ASSERT_TRUE(cfg_.backend_.initialize(cfg_.geometry_));
        ASSERT_TRUE(cfg_.backend_.open());
    }

    void TearDown() override {
        ASSERT_TRUE(cfg_.backend_.close());
    }
};

typedef ::testing::Types<StorageBackendTreeConfiguration, InMemoryTreeConfiguration> Implementations;

TYPED_TEST_CASE(PersistedTreeSuite, Implementations);

TYPED_TEST(PersistedTreeSuite, BuildTree) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

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

TYPED_TEST(PersistedTreeSuite, Remove) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

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

TYPED_TEST(PersistedTreeSuite, MultipleLookupRandom) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

    std::map<typename TypeParam::NodeType::KeyType, typename TypeParam::NodeType::ValueType> map;

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

TYPED_TEST(PersistedTreeSuite, MultipleLookupCustomKeyType) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

    std::map<typename TypeParam::NodeType::KeyType, typename TypeParam::NodeType::ValueType> map;
    std::vector<uint32_t> inodes;

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

TYPED_TEST(PersistedTreeSuite, FindLessThanLookup) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

    std::map<typename TypeParam::NodeType::KeyType, typename TypeParam::NodeType::ValueType> map;
    std::map<uint32_t, uint32_t> last_offsets;
    std::vector<uint32_t> inodes;

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
        typename TypeParam::NodeType::KeyType found;
        typename TypeParam::NodeType::ValueType value;

        auto key = INodeKey(inode, UINT32_MAX);

        ASSERT_TRUE(tree.find_less_then(key, &value, &found));

        auto key_offset = (found) & ((uint32_t)-1);

        EXPECT_EQ(last_offsets[inode], key_offset);
    }
}

template<typename NodeType, typename NodeRefType>
struct SimpleVisitor : PersistedTreeVisitor<NodeRefType, NodeType> {
    int32_t calls = 0;

    virtual void visit(NodeRefType nref, NodeType *node) override {
        calls++;
    }
};

TYPED_TEST(PersistedTreeSuite, WalkSmallTree) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

    tree.add(100, 5738);
    tree.add(10, 1);
    tree.add(22, 2);
    tree.add(8, 3);
    tree.add(3, 4);
    tree.add(17, 5);
    tree.add(9, 6);
    tree.add(30, 7);

    SimpleVisitor<typename TypeParam::NodeType, typename TypeParam::NodeRefType> visitor;

    tree.accept(visitor);

    ASSERT_EQ(visitor.calls, 3);
}


TYPED_TEST(PersistedTreeSuite, WalkLargeTree) {
    PersistedTree<typename TypeParam::NodeType> tree{ this->cfg_.cache_ };

    std::map<typename TypeParam::NodeType::KeyType, typename TypeParam::NodeType::ValueType> map;
    std::map<uint32_t, uint32_t> last_offsets;
    std::vector<uint32_t> inodes;

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

    SimpleVisitor<typename TypeParam::NodeType, typename TypeParam::NodeRefType> visitor;

    tree.accept(visitor);

    ASSERT_EQ(visitor.calls, 494);
}
