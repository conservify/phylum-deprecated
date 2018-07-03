#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "phylum/preallocated.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class FileIndexSuite : public ::testing::Test {

};

TEST_F(FileIndexSuite, LargeIndex) {
    auto number_of_index_entries = 1024 * 1024;
    auto bytes_required = number_of_index_entries * sizeof(IndexRecord);
    auto blocks_required = uint32_t(bytes_required / ((4 * 4 * SectorSize) - SectorSize - SectorSize)) * 2;

    Geometry geometry{ blocks_required, 4, 4, 512 };
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
        index.append(i * 1024, addr);
        addr.add(1024);
    }

    ASSERT_EQ(helper.number_of_blocks(BlockType::Index, 0, blocks_required - 1), 4388);
}