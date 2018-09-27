#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "phylum/file_layout.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class AllocationSuite : public ::testing::Test {
};

TEST_F(AllocationSuite, SdCardSizeCalculatedCorrectly) {
    auto number_of_sd_blocks = UINT32_MAX / SectorSize;
    auto g = Geometry::from_physical_block_layout(number_of_sd_blocks, SectorSize);

    ASSERT_EQ(g.number_of_blocks, number_of_sd_blocks / (4 * 4));
}

TEST_F(AllocationSuite, FormattingLayoutLargerThanGeometry) {
    Geometry geometry{ 484032, 4, 4, 512 };

    FileDescriptor file_system_area_fd = { "system",          100  };
    FileDescriptor file_emergency_fd   = { "emergency.fklog", 100  };
    FileDescriptor file_logs_a_fd =      { "logs-a.fklog",    2048 };
    FileDescriptor file_logs_b_fd =      { "logs-b.fklog",    2048 };
    FileDescriptor file_data_fk =        { "data.fk",         0    };
    FileDescriptor* files[5]{
        &file_system_area_fd,
        &file_emergency_fd,
        &file_logs_a_fd,
        &file_logs_b_fd,
        &file_data_fk
    };

    FilePreallocator allocator{ geometry };

    FileAllocation allocation;
    ASSERT_TRUE(allocator.allocate(0, files[0], allocation));
    ASSERT_TRUE(allocator.allocate(1, files[1], allocation));
    ASSERT_TRUE(allocator.allocate(2, files[2], allocation));
    ASSERT_FALSE(allocator.allocate(3, files[3], allocation));
    ASSERT_FALSE(allocator.allocate(4, files[4], allocation));
}

