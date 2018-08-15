#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "phylum/file_layout.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class FileIndexSuite : public ::testing::Test {
};

class StandardFileIndexSuite : public FileIndexSuite {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    FileAllocation fa_{ { 1, 1023 }, { 0, 0 } };
    BlockHelper helper_{ storage_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

};

TEST_F(StandardFileIndexSuite, FormatNew) {
    FileIndex index{ &storage_, &fa_ };

    ASSERT_TRUE(index.format());

    ASSERT_EQ(helper_.number_of_blocks(BlockType::Index), 1);
}

TEST_F(FileIndexSuite, LargeIndex) {
    auto number_of_index_entries = 1024 * 1024;
    auto bytes_required = number_of_index_entries * sizeof(IndexRecord);
    auto blocks_required = uint32_t(bytes_required / ((4 * 4 * SectorSize) - SectorSize - SectorSize));

    Geometry geometry{ blocks_required, 4, 4, SectorSize };
    LinuxMemoryBackend storage;
    FileAllocation allocation{ { 1, blocks_required }, { 0, 1 } };
    FileIndex index{ &storage, &allocation };
    BlockHelper helper{ storage };

    ASSERT_TRUE(storage.initialize(geometry));
    ASSERT_TRUE(storage.open());

    ASSERT_TRUE(index.format());

    ASSERT_EQ(helper.number_of_blocks(BlockType::Index, 0, blocks_required - 1), 1);

    auto addr = BlockAddress{ 100000, 0 };

    for (auto i = 0; i < number_of_index_entries; ++i) {
        index.append(i, addr);
        addr.add(1);
    }

    ASSERT_EQ(helper.number_of_blocks(BlockType::Index, 0, blocks_required - 1), 4388);

    storage.log().clear();
    ASSERT_TRUE(index.initialize());
    ASSERT_EQ(storage.log().size(), 19);

    IndexRecord record;

    storage.log().clear();
    ASSERT_TRUE(index.seek(number_of_index_entries / 2, record));
    ASSERT_EQ(storage.log().size(), 24);
    ASSERT_EQ(record.position, (uint64_t)(number_of_index_entries / 2));

    storage.log().clear();
    ASSERT_TRUE(index.seek(0, record));
    ASSERT_EQ(storage.log().size(), 14);
    ASSERT_EQ(record.position, (uint64_t)0);
}
