#include <gtest/gtest.h>

#include "phylum/super_block.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class SuperBlockSuite : public ::testing::Test {
protected:
    static constexpr int32_t anchor_overflow_iterations = 15 * 15 * 15 * 15 + 6;

protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    BlockAllocator allocator_{ storage_ };
    SuperBlockManager sbm_{ storage_, allocator_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

    void TearDown() override {
        ASSERT_TRUE(storage_.close());
    }

};

TEST_F(SuperBlockSuite, LocatingUnformatted) {
    storage_.randomize();

    ASSERT_FALSE(sbm_.locate());
}

TEST_F(SuperBlockSuite, Formatting) {
    ASSERT_TRUE(sbm_.create());
    ASSERT_TRUE(sbm_.locate());
}

TEST_F(SuperBlockSuite, SavingAFewRevisions) {
    ASSERT_TRUE(sbm_.create());

    for (auto i = 0; i < 5; ++i) {
        ASSERT_TRUE(sbm_.save());
    }

    ASSERT_EQ(sbm_.location().sector, 6);

    ASSERT_TRUE(sbm_.locate());

    ASSERT_EQ(sbm_.location().sector, 6);
}

TEST_F(SuperBlockSuite, BlockRollover) {
    ASSERT_TRUE(sbm_.create());

    auto old = sbm_.location();

    for (auto i = 0; i < 18; ++i) {
        ASSERT_TRUE(sbm_.save());
    }

    ASSERT_TRUE(sbm_.locate());

    ASSERT_NE(sbm_.location().block, old.block);
    ASSERT_EQ(sbm_.location().sector, 4);
}

TEST_F(SuperBlockSuite, AnchorAreaRollover) {
    ASSERT_TRUE(sbm_.create());

    auto old = sbm_.location();

    for (auto i = 0; i < anchor_overflow_iterations; ++i) {
        ASSERT_TRUE(sbm_.save());
    }

    ASSERT_TRUE(sbm_.locate());

    ASSERT_NE(sbm_.location().block, old.block);
    ASSERT_EQ(sbm_.location().sector, 7);
}

TEST_F(SuperBlockSuite, AnchorAreaRolloverTwice) {
    ASSERT_TRUE(sbm_.create());

    auto old = sbm_.location();

    for (auto i = 0; i < anchor_overflow_iterations * 2; ++i) {
        ASSERT_TRUE(sbm_.save());
    }

    ASSERT_TRUE(sbm_.locate());

    ASSERT_NE(sbm_.location().block, old.block);
    ASSERT_EQ(sbm_.location().sector, 13);
}
