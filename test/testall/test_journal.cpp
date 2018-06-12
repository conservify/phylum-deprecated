#include <gtest/gtest.h>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class JournalSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    SequentialBlockAllocator allocator_{ geometry_ };
    FileSystem fs_{ storage_, allocator_ };
    BlockHelper helper_{ storage_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(fs_.initialize(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.close());
    }

};

TEST_F(JournalSuite, CreatesEmptyJournal) {
    ASSERT_TRUE(helper_.is_type(fs_.sb().journal, BlockType::Journal));
}
