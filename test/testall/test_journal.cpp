#include <gtest/gtest.h>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class JournalSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_{ geometry_ };
    FileSystem fs_{ storage_, allocator_ };
    BlockHelper helper_{ storage_, allocator_ };

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

TEST_F(JournalSuite, AppendingAFewEntries) {
    for (auto i = 0; i < 10; ++i) {
        ASSERT_TRUE(fs_.journal().append({ JournalEntryType::Allocation, (block_index_t)(i + 10) }));
    }

    BlockAddress expected{ 7, 1 * SectorSize + 120 };
    ASSERT_EQ(fs_.journal().location(), expected);
}

TEST_F(JournalSuite, AppendingEntriesIntoFollowingBlock) {
    auto entry_size = (int32_t)sizeof(JournalEntry);
    auto entries_per_block = (int32_t)geometry_.block_size() / entry_size;

    for (auto i = 0; i < entries_per_block + 6; ++i) {
        ASSERT_TRUE(fs_.journal().append({ JournalEntryType::Allocation, (block_index_t)(i + 10) }));
    }

    BlockAddress expected{ 9, 2 * SectorSize + 192 };
    ASSERT_EQ(fs_.journal().location(), expected);
}

TEST_F(JournalSuite, FindsEndOfJournalFromFirstBlock) {
    auto entry_size = (int32_t)sizeof(JournalEntry);
    auto entries_per_block = (int32_t)geometry_.block_size() / entry_size;

    auto initial_location = fs_.journal().location();

    for (auto i = 0; i < 2 * entries_per_block + 6; ++i) {
        ASSERT_TRUE(fs_.journal().append({ JournalEntryType::Allocation, (block_index_t)(i + 10) }));
    }

    BlockAddress expected{ 10, 3 * SectorSize + 312 };
    ASSERT_EQ(fs_.journal().location(), expected);

    Journal journal{ storage_, allocator_ };
    ASSERT_TRUE(journal.locate(initial_location.block));

    ASSERT_EQ(journal.location(), expected);
}
