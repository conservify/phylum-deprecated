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
    const Geometry &geometry() const {
        return GetParam().geometry;
    }

    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry()));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.mount(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.unmount());
    }

};

TEST_P(VaryingDeviceSuite, Mounting) {
}

TEST_P(VaryingDeviceSuite, WriteFileToHalfTheSpace) {
    uint8_t data[512] = { 0xcc };

    auto file = fs_.open("large.bin");
    ASSERT_TRUE(file.open());

    auto size = int32_t(storage_.size() / 2);
    auto written = 0;
    while (written < size) {
        if (file.write(data, sizeof(data)) != sizeof(data)) {
            break;
        }

        written += sizeof(data);
    }

    file.close();
}

TEST_P(VaryingDeviceSuite, WriteSmallerFilesToHalfTheSpace) {
    uint8_t data[512] = { 0xcc };

    auto number_of_files = 10;
    auto per_file = int32_t(storage_.size() / 2) / number_of_files;

    for (auto i = 0; i < number_of_files; ++i) {
        std::ostringstream fn;
        fn << "large-" << i << ".bin";

        auto file = fs_.open(fn.str().c_str());
        ASSERT_TRUE(file.open());

        auto size = per_file;
        auto written = 0;
        while (written < size) {
            if (file.write(data, sizeof(data)) != sizeof(data)) {
                break;
            }
            written += sizeof(data);
        }

        file.close();
    }
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

INSTANTIATE_TEST_CASE_P(SmallerDevices, VaryingDeviceSuite, ::testing::Values(SmallMemory), );
INSTANTIATE_TEST_CASE_P(LargeDevices, VaryingDeviceSuite, ::testing::Values(SdCard4gb, SdCard8gb), );
