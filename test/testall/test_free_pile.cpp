#include <gtest/gtest.h>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class FreePileSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_;
    FileSystem fs_{ storage_, allocator_ };
    BlockHelper helper_{ storage_, allocator_ };

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

TEST_F(FreePileSuite, CreatesEmptyFreePile) {
    ASSERT_TRUE(helper_.is_type(fs_.sb().free, BlockType::Free));
}

TEST_F(FreePileSuite, AppendingAFewEntries) {
    auto before = fs_.fpm().location();

    for (auto i = 0; i < 10; ++i) {
        ASSERT_TRUE(fs_.fpm().append({ (block_index_t)(i + 10) }));
    }

    auto after = fs_.fpm().location();
    ASSERT_EQ(before.block, after.block);
    ASSERT_NE(before.position, after.position);
}

TEST_F(FreePileSuite, AppendingEntriesIntoFollowingBlock) {
    auto entry_size = (int32_t)sizeof(FreePileEntry);
    auto entries_per_block = (int32_t)geometry_.block_size() / entry_size;
    auto before = fs_.fpm().location();

    for (auto i = 0; i < entries_per_block + 6; ++i) {
        ASSERT_TRUE(fs_.fpm().append({ (block_index_t)(i + 10) }));
    }

    auto after = fs_.fpm().location();
    ASSERT_NE(before.block, after.block);
    ASSERT_NE(before.position, after.position);
}

TEST_F(FreePileSuite, FindsEndOfFreePileFromFirstBlock) {
    auto entry_size = (int32_t)sizeof(FreePileEntry);
    auto entries_per_block = (int32_t)geometry_.block_size() / entry_size;
    auto before = fs_.fpm().location();

    for (auto i = 0; i < 2 * entries_per_block + 6; ++i) {
        ASSERT_TRUE(fs_.fpm().append({ (block_index_t)(i + 10) }));
    }

    auto after = fs_.fpm().location();
    ASSERT_NE(before.block, after.block);
    ASSERT_NE(before.position, after.position);

    FreePileManager fpm{ storage_, allocator_ };
    ASSERT_TRUE(fpm.locate(before.block));

    ASSERT_EQ(fpm.location(), after);
}
