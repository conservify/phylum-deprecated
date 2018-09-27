#include <gtest/gtest.h>

#include "phylum/tree_fs_super_block.h"
#include "phylum/files.h"
#include "phylum/unused_block_reclaimer.h"
#include "phylum/basic_super_block_manager.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

struct SimpleState : phylum::MinimumSuperBlock {
    uint32_t time;
};

class SuperBlockNonStandardSizeSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 32, 32, 4, 2048 }; // 2MB Serial Flash
    LinuxMemoryBackend storage_;
    SerialFlashAllocator allocator_{ storage_ };
    BasicSuperBlockManager<SimpleState> manager_{ storage_, allocator_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(allocator_.initialize());
    }

    void TearDown() override {
        ASSERT_TRUE(storage_.close());
    }

};

TEST_F(SuperBlockNonStandardSizeSuite, LocatingUnformatted) {
    storage_.randomize();

    ASSERT_FALSE(manager_.locate());
}

TEST_F(SuperBlockNonStandardSizeSuite, Formatting) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());
}

TEST_F(SuperBlockNonStandardSizeSuite, SavingAFewRevisions) {
    ASSERT_TRUE(manager_.create());

    for (auto i = 0; i < 5; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    ASSERT_EQ(manager_.location().sector, 5);

    BasicSuperBlockManager<SimpleState> other_manager{ storage_, allocator_ };

    ASSERT_EQ(other_manager.location().block, BLOCK_INDEX_INVALID);
    ASSERT_EQ(other_manager.location().sector, SECTOR_INDEX_INVALID);

    ASSERT_TRUE(other_manager.locate());

    ASSERT_EQ(other_manager.location().block, 3);
    ASSERT_EQ(other_manager.location().sector, 5);
}

