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
    Geometry geometry_{ 32, 8, 4, 2048 }; // 2MB Serial Flash
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

    ASSERT_EQ(other_manager.location().block, (block_index_t)31);
    ASSERT_EQ(other_manager.location().sector, (sector_index_t)5);
}

TEST_F(SuperBlockNonStandardSizeSuite, PrefersInvalidBlocksDuringAllocation) {
    ASSERT_TRUE(manager_.create());

    for (block_index_t i = 0; i < geometry_.number_of_blocks; ++i) {
        if (!allocator_.is_taken(i)) {
            if (i != 10) {
                ASSERT_TRUE(allocator_.free(i, 1));
            }
        }
    }

    for (auto i = 0; i < 33; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    ASSERT_EQ(manager_.location().block, (block_index_t)10);

    BlockHead header;
    ASSERT_TRUE(storage_.read(BlockAddress{ manager_.location().block, 0 }, &header, sizeof(BlockHead)));
    ASSERT_EQ(header.age, (block_age_t)1);
}

TEST_F(SuperBlockNonStandardSizeSuite, AgeIsIncrementedWhenAllocatingBlockWithAge) {
    ASSERT_TRUE(manager_.create());

    for (block_index_t i = 0; i < geometry_.number_of_blocks; ++i) {
        if (!allocator_.is_taken(i)) {
            ASSERT_TRUE(allocator_.free(i, 10));
        }
    }

    storage_.log().logging(false);

    for (auto i = 0; i < 33; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    storage_.log().logging(false);

    ASSERT_EQ(manager_.location().block, (block_index_t)3);

    BlockHead header;
    ASSERT_TRUE(storage_.read(BlockAddress{ manager_.location().block, 0 }, &header, sizeof(BlockHead)));
    ASSERT_EQ(header.age, (block_age_t)11);
}
