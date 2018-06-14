#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class GarbageCollectionSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_{ geometry_ };
    FileSystem fs_{ storage_, allocator_ };
    DataHelper helper{ fs_ };
    BlockHelper blocks{ storage_, allocator_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.mount(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.unmount());
    }

};

TEST_F(GarbageCollectionSuite, RunOnEmpty) {
    ASSERT_TRUE(fs_.gc());
}

TEST_F(GarbageCollectionSuite, RunOnSingleBlockTree) {
    ASSERT_TRUE(helper.write_file("test-1.bin", 32));
    ASSERT_TRUE(helper.write_file("test-2.bin", 32));

    ASSERT_EQ(blocks.number_of_blocks(BlockType::Leaf), 1);
    ASSERT_EQ(blocks.number_of_blocks(BlockType::Index), 0);

    auto before = fs_.sb().tree;

    ASSERT_TRUE(fs_.gc());

    auto after = fs_.sb().tree;

    ASSERT_NE(before, after);

    ASSERT_EQ(blocks.number_of_blocks(BlockType::Leaf), 2);
    ASSERT_EQ(blocks.number_of_blocks(BlockType::Index), 0);
}

TEST_F(GarbageCollectionSuite, RunOnSingleLargeTree) {
    ASSERT_TRUE(helper.write_file("test-1.bin", geometry_.block_size() * 256));
    ASSERT_TRUE(helper.write_file("test-2.bin", geometry_.block_size() * 256));

    ASSERT_EQ(blocks.number_of_blocks(BlockType::Leaf), 2);
    ASSERT_EQ(blocks.number_of_blocks(BlockType::Index), 3);

    auto before_address = fs_.sb().tree;
    auto before_ts = fs_.sb().last_gc;

    ASSERT_TRUE(fs_.gc());

    auto after_address = fs_.sb().tree;
    auto after_ts = fs_.sb().last_gc;

    ASSERT_NE(before_address, after_address);
    ASSERT_GT(after_ts, before_ts);

    ASSERT_EQ(blocks.number_of_blocks(BlockType::Leaf), 3);
    ASSERT_EQ(blocks.number_of_blocks(BlockType::Index), 4);
}
