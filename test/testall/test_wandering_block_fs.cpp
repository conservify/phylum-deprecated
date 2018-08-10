#include <gtest/gtest.h>

#include "phylum/super_block.h"
#include "backends/linux_memory/linux_memory.h"
#include "backends/arduino_serial_flash/serial_flash_allocator.h"

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

TEST_F(WanderingBlockSuite, CreatingFile) {
    ASSERT_TRUE(manager_.create());
    ASSERT_TRUE(manager_.locate());
}
