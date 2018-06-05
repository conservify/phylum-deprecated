#include <gtest/gtest.h>

#include "utilities.h"

class AddressingSuite : public ::testing::Test {
};

TEST_F(AddressingSuite, Iterator) {
    Geometry g{ 1024, 4, 4, 512 };
    BlockIterator iter{ 0 };

    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size());
    ASSERT_EQ(iter.sector_offset(g), 0);

    iter.add(128);

    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size - 128);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - 128);
    ASSERT_EQ(iter.sector_offset(g), 128);

    iter.add(512);

    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size - 128);
    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - 128 - 512);
    ASSERT_EQ(iter.sector_offset(g), 128);

    auto pos = 512 * 6 + 36;
    iter.seek(pos);

    ASSERT_EQ(iter.remaining_in_block(g), g.block_size() - pos);
    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size - 36);
    ASSERT_EQ(iter.sector_offset(g), 36);

    iter.seek(500);

    ASSERT_EQ(iter.remaining_in_sector(g), g.sector_size - 500);

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
