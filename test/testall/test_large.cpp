#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

struct TestConfiguration {
    Geometry geometry;
};

class VaryingDeviceSuite : public ::testing::TestWithParam<TestConfiguration> {
protected:
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_;
    FileSystem fs_{ storage_, allocator_ };

protected:
    void SetUp() override {
        auto p = GetParam();

        ASSERT_TRUE(storage_.initialize(p.geometry));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.mount(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.unmount());
    }

};

TEST_P(VaryingDeviceSuite, Mounting) {
}

static Geometry from_disk_size(uint64_t size) {
    auto number_of_sectors = size / (uint64_t)SectorSize;
    auto pages_per_block = (page_index_t)4;
    auto sectors_per_page = (page_index_t)4;
    auto number_of_blocks = (uint32_t)(number_of_sectors / (pages_per_block * sectors_per_page));
    return {
        number_of_blocks,
        pages_per_block,
        sectors_per_page,
        SectorSize
    };
}

static TestConfiguration SmallMemory{ from_disk_size((uint64_t)128 * 1024 * 1024) };
static TestConfiguration SdCard4gb{ from_disk_size((uint64_t)4 * 1024 * 1024 * 1024) };
static TestConfiguration SdCard8gb{ from_disk_size((uint64_t)8 * 1024 * 1024 * 1024) };

INSTANTIATE_TEST_CASE_P(GeneralDevices, VaryingDeviceSuite,
                        ::testing::Values(SmallMemory, SdCard4gb, SdCard8gb), );

