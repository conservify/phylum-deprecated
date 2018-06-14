#include <gtest/gtest.h>

#include "phylum/block_alloc.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class AddressingSuite : public ::testing::Test {
};

TEST_F(AddressingSuite, AddressIterating) {
    Geometry g{ 1024, 4, 4, 512 };
    BlockAddress iter{ 0, 0 };

    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size());
    ASSERT_EQ(iter.sector_offset(g), 0);

    iter.add(128);

    ASSERT_EQ(iter.remaining_in_sector(g), (uint32_t)g.sector_size - 128);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - 128);
    ASSERT_EQ(iter.sector_offset(g), 128);

    iter.add(512);

    ASSERT_EQ(iter.remaining_in_sector(g), (uint32_t)g.sector_size - 128);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - 128 - 512);
    ASSERT_EQ(iter.sector_offset(g), 128);

    auto pos = 512 * 6 + 36;
    iter.seek(pos);

    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - pos);
    ASSERT_EQ(iter.remaining_in_sector(g), (uint32_t)g.sector_size - 36);
    ASSERT_EQ(iter.sector_offset(g), 36);

    iter.seek(500);

    ASSERT_EQ(iter.remaining_in_sector(g), (uint32_t)g.sector_size - 500);

    ASSERT_TRUE(iter.find_room(g, 36));

    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - 512);
    ASSERT_EQ(iter.sector_offset(g), 0);

    ASSERT_TRUE(iter.find_room(g, 128));

    ASSERT_EQ(iter.sector_offset(g), 0);

    iter.add(128);

    ASSERT_TRUE(iter.find_room(g, 128));

    ASSERT_EQ(iter.sector_offset(g), 128);

    iter.seek(g.block_size() - 128);

    ASSERT_FALSE(iter.find_room(g, 384));

    ASSERT_TRUE(iter.find_room(g, 128));
}

TEST_F(AddressingSuite, FindRoomAtEndOfBlock) {
    Geometry g{ 1024, 4, 4, 512 };
    BlockAddress iter{ 0, 0 };

    // This checks for a bug where we tested the sector remaining before block
    // remaining which assumed our sector was valid.
    iter.add(iter.remaining_in_block(g));

    ASSERT_FALSE(iter.find_room(g, 128));
}

TEST_F(AddressingSuite, FindEndAfterFillingBlockAndBeforeStartingNext) {
    uint8_t pattern[128];
    Geometry g{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage;
    DebuggingBlockAllocator allocator{ g };

    ASSERT_TRUE(storage.initialize(g));
    ASSERT_TRUE(storage.open());

    auto first_block = allocator.allocate(BlockType::File);

    auto layout = BlockLayout<TreeBlockHead, TreeBlockTail>{ storage, allocator, BlockAddress{ first_block, 0 }, BlockType::File };

    for (auto i = 0; i < ((512 * (4 * 4 - 1)) / 128) - 1; i++) {
        auto address = layout.find_available(sizeof(pattern));
        auto remaining = address.remaining_in_block(g) - sizeof(pattern);

        ASSERT_TRUE(address.valid());
        ASSERT_TRUE(storage.write(address, pattern, sizeof(pattern)));

        if (remaining <= sizeof(pattern)) {
            break;
        }
    }

    ASSERT_LE(layout.address().remaining_in_block(g), sizeof(pattern));

    auto fn = [&](StorageBackend &storage, BlockAddress& address) -> bool {
        auto remaining = address.remaining_in_block(g) - sizeof(TreeBlockTail);
        return remaining >= sizeof(pattern);
    };

    ASSERT_TRUE(layout.find_tail_entry(first_block, sizeof(pattern), fn));
}
