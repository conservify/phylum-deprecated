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

    for (auto i = 0; i < 33; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    ASSERT_EQ(manager_.location().block, (block_index_t)3);

    BlockHead header;
    ASSERT_TRUE(storage_.read(BlockAddress{ manager_.location().block, 0 }, &header, sizeof(BlockHead)));
    ASSERT_EQ(header.age, (block_age_t)11);
}

TEST_F(SuperBlockNonStandardSizeSuite, TimestampWrapAround) {
    ASSERT_TRUE(manager_.create());

    for (block_index_t i = 0; i < geometry_.number_of_blocks; ++i) {
        if (!allocator_.is_taken(i)) {
            ASSERT_TRUE(allocator_.free(i, 10));
        }
    }

    manager_.state().link.header.timestamp = BLOCK_AGE_INVALID - 10;

    for (auto i = 0; i < 12; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    BasicSuperBlockManager<SimpleState> other_manager{ storage_, allocator_ };

    ASSERT_TRUE(other_manager.locate());

    ASSERT_EQ(other_manager.state().link.header.timestamp, (sector_index_t)1);
}

TEST_F(SuperBlockNonStandardSizeSuite, WritingAndReadingFile) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    Files files{ &storage_, &allocator_ };

    auto file1 = files.open({ }, OpenMode::Write);

    ASSERT_TRUE(file1.initialize());
    ASSERT_FALSE(file1.exists());
    ASSERT_TRUE(file1.format());

    auto location = file1.beginning();

    PatternHelper helper;
    auto total = helper.write(file1, (1024) / helper.size());
    file1.close();

    ASSERT_EQ(total, (uint32_t)(1024));

    auto file2 = files.open(location, OpenMode::Read);
    ASSERT_TRUE(file2.exists());

    file2.seek(0);
    ASSERT_EQ(file2.tell(), (uint32_t)0);
    auto verified = helper.read(file2);
    ASSERT_EQ(verified, (uint32_t)(1024));

    ASSERT_EQ(allocator_.number_of_free_blocks(), (uint32_t)25);
}

TEST_F(SuperBlockNonStandardSizeSuite, Preallocating) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    Files files{ &storage_, &allocator_ };

    ASSERT_TRUE(allocator_.preallocate(4));

    auto file1 = files.open({ }, OpenMode::Write);

}

TEST_F(SuperBlockNonStandardSizeSuite, WritingFileAndCheckingSize) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    Files files{ &storage_, &allocator_ };

    auto file1 = files.open({ }, OpenMode::Write);

    ASSERT_TRUE(file1.initialize());
    ASSERT_TRUE(file1.format());

    auto location = file1.beginning();

    auto total = 0;
    uint8_t buffer[163] = { 0xee };

    for (auto i = 0; i < 388; ++i) {
        auto wrote = file1.write(buffer, sizeof(buffer), true);
        ASSERT_TRUE(wrote > 0);
        total += wrote;
    }

    auto wrote = file1.write(buffer, 16, true);
    ASSERT_TRUE(wrote > 0);
    total += wrote;

    ASSERT_EQ((uint32_t)total, (uint32_t)63260);

    file1.close();

    auto file2 = files.open(location, OpenMode::Read);
    file2.seek(UINT64_MAX);
    ASSERT_EQ(file2.size(), (uint32_t)63260);
}
