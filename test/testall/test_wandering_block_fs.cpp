#include <gtest/gtest.h>

#include "phylum/super_block.h"
#include "phylum/serial_flash_fs.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

struct SimpleState : phylum::MinimumSuperBlock {
    uint32_t time;
};

class WanderingBlockSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 32, 32, 4, 512 }; // 2MB Serial Flash
    LinuxMemoryBackend storage_;
    SerialFlashAllocator allocator_{ storage_ };
    SerialFlashStateManager<SimpleState> manager_{ storage_, allocator_ };

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

TEST_F(WanderingBlockSuite, LocatingUnformatted) {
    storage_.randomize();

    ASSERT_FALSE(manager_.locate());
}

TEST_F(WanderingBlockSuite, Formatting) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());
}

TEST_F(WanderingBlockSuite, SavingAFewRevisions) {
    ASSERT_TRUE(manager_.create());

    for (auto i = 0; i < 5; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    ASSERT_EQ(manager_.location().sector, 5);

    ASSERT_TRUE(manager_.locate());

    ASSERT_EQ(manager_.location().sector, 5);
}

TEST_F(WanderingBlockSuite, CreatingSmallFile) {
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
    ASSERT_EQ(file2.tell(), (uint32_t)0);
    auto verified = helper.read(file2);
    ASSERT_EQ(verified, (uint32_t)(1024));

    ASSERT_EQ(allocator_.number_of_free_blocks(), (uint32_t)25);
}

TEST_F(WanderingBlockSuite, CreatingLargeFile) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    Files files{ &storage_, &allocator_ };

    auto file1 = files.open({ }, OpenMode::Write);

    ASSERT_TRUE(file1.initialize());
    ASSERT_FALSE(file1.exists());
    ASSERT_TRUE(file1.format());

    auto location = file1.beginning();

    PatternHelper helper;
    auto total = helper.write(file1, (1024 * 1024) / helper.size());
    file1.close();

    ASSERT_EQ(total, (uint32_t)(1024 * 1024));

    auto file2 = files.open(location, OpenMode::Read);
    ASSERT_TRUE(file2.exists());
    ASSERT_EQ(file2.tell(), (uint32_t)0);
    auto verified = helper.read(file2);
    ASSERT_EQ(verified, (uint32_t)(1024 * 1024));

    ASSERT_EQ(allocator_.number_of_free_blocks(), (uint32_t)9);
}

struct CountingVisitor : BlockVisitor {
    uint32_t counter{ 0 };

    void block(block_index_t block) override {
        counter++;
    }

};

TEST_F(WanderingBlockSuite, WalkingBlocksOfSmallFile) {
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

    CountingVisitor visitor;
    auto file2 = files.open(location, OpenMode::Read);
    file2.walk(&visitor);

    ASSERT_EQ(allocator_.number_of_free_blocks(), (uint32_t)25);
    ASSERT_EQ(visitor.counter, (uint32_t)1);
}

TEST_F(WanderingBlockSuite, WalkingBlocksOfLargeFile) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    Files files{ &storage_, &allocator_ };

    auto file1 = files.open({ }, OpenMode::Write);

    ASSERT_TRUE(file1.initialize());
    ASSERT_FALSE(file1.exists());
    ASSERT_TRUE(file1.format());

    auto location = file1.beginning();

    PatternHelper helper;
    auto total = helper.write(file1, (1024 * 1024) / helper.size());
    file1.close();

    ASSERT_EQ(total, (uint32_t)(1024 * 1024));

    CountingVisitor visitor;
    auto file2 = files.open(location, OpenMode::Read);
    file2.walk(&visitor);

    ASSERT_EQ(allocator_.number_of_free_blocks(), (uint32_t)9);
    ASSERT_EQ(visitor.counter, (uint32_t)17);
}

TEST_F(WanderingBlockSuite, UnusedBlockReclaim) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());

    for (auto i = 0; i < 128; ++i) {
        ASSERT_TRUE(manager_.save());
    }

    Files files{ &storage_, &allocator_ };
    auto file1 = files.open({ }, OpenMode::Write);
    ASSERT_TRUE(file1.format());
    auto location = file1.beginning();
    PatternHelper helper;
    helper.write(file1, (1024 * 256) / helper.size());
    file1.close();

    UnusedBlockReclaimer reclaimer(&files, &manager_);
    reclaimer.walk(location);
    reclaimer.reclaim();
}
