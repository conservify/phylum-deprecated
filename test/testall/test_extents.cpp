#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "phylum/preallocated.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class ExtentsSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

    void TearDown() override {
    }

};

TEST_F(ExtentsSuite, StandardLayoutAllocating) {
    FileDescriptor file_system_area_fd =   { "system",        WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd =   { "startup.log",   WriteStrategy::Append,  100 };
    FileDescriptor file_log_now_fd =       { "now.log",       WriteStrategy::Rolling, 100 };
    FileDescriptor file_log_emergency_fd = { "emergency.log", WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk",       WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
        &file_log_now_fd,
        &file_log_emergency_fd,
        &file_data_fk
    };

    FileLayout<5> layout{ storage_ };

    FileAllocation expected[] = {
        FileAllocation{ Extent{  2, 2 }, Extent{  4, 14 } },
        FileAllocation{ Extent{ 18, 2 }, Extent{ 20, 14 } }
    };

    ASSERT_FALSE(layout.mount(files));
    ASSERT_TRUE(layout.unmount());

    ASSERT_TRUE(layout.format(files));
    ASSERT_EQ(layout.allocation(0), expected[0]);
    ASSERT_EQ(layout.allocation(1), expected[1]);

    ASSERT_TRUE(layout.unmount());
    ASSERT_EQ(layout.allocation(0), FileAllocation{ });
    ASSERT_EQ(layout.allocation(1), FileAllocation{ });

    ASSERT_TRUE(layout.mount(files));
    ASSERT_EQ(layout.allocation(0), expected[0]);
    ASSERT_EQ(layout.allocation(1), expected[1]);

    ASSERT_TRUE(layout.unmount());
    ASSERT_EQ(layout.allocation(0), FileAllocation{ });
    ASSERT_EQ(layout.allocation(1), FileAllocation{ });
}

TEST_F(ExtentsSuite, SmallFileWritingToEnd) {
    FileDescriptor file_system_area_fd = { "system",      WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd = { "startup.log", WriteStrategy::Append,  100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_log_startup_fd, OpenMode::Write);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());

    ASSERT_EQ(total, file.maximum_size());

    file.close();

    auto verified = helper.verify_file(layout, file_log_startup_fd);
    ASSERT_EQ(total, verified);
}

TEST_F(ExtentsSuite, LargeFileWritingToEnd) {
    FileDescriptor file_system_area_fd = { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =        { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_data_fk, OpenMode::Write);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());

    file.close();

    ASSERT_EQ(total, (uint64_t)1024 * 1024);

    auto verified = helper.verify_file(layout, file_data_fk);
    ASSERT_EQ(total, verified);
}

TEST_F(ExtentsSuite, LargeFileAppending) {
    FileDescriptor file_system_area_fd = { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =        { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;

    PatternHelper helper;
    {
        auto file = layout.open(file_data_fk, OpenMode::Write);
        ASSERT_TRUE(file);
        ASSERT_EQ(file.size(), (uint64_t)0);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    {
        auto file = layout.open(file_data_fk, OpenMode::Write);
        ASSERT_TRUE(file);
        ASSERT_TRUE(file.seek(UINT64_MAX));
        ASSERT_EQ(file.size(), OneMegabyte);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), 2 * OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    auto verified = helper.verify_file(layout, file_data_fk);
    ASSERT_EQ(OneMegabyte * 2, verified);
}

TEST_F(ExtentsSuite, Seeking) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;
    auto file = layout.open(file_data_fk, OpenMode::Write);
    ASSERT_EQ(file.size(), (uint64_t)0);
    PatternHelper helper;
    auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());
    file.close();

    ASSERT_EQ(file.size(), OneMegabyte);
    ASSERT_EQ(total, OneMegabyte);

    auto reading = layout.open(file_data_fk);
    ASSERT_EQ(reading.size(), (uint64_t)0);
    reading.seek(UINT64_MAX);
    ASSERT_EQ(reading.size(), (uint64_t)OneMegabyte);
    reading.seek(0);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte);
}

TEST_F(ExtentsSuite, SeekMiddleOfFile) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;
    auto file = layout.open(file_data_fk, OpenMode::Write);
    ASSERT_EQ(file.size(), (uint64_t)0);
    PatternHelper helper;
    auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());
    file.close();

    ASSERT_EQ(file.size(), OneMegabyte);
    ASSERT_EQ(total, OneMegabyte);

    auto middle_on_pattern_edge = ((OneMegabyte / 2) / helper.size()) * helper.size();
    auto reading = layout.open(file_data_fk);
    ASSERT_EQ(reading.size(), (uint64_t)0);
    storage_.log().logging(true);
    ASSERT_TRUE(reading.seek(middle_on_pattern_edge));
    storage_.log().logging(false);
    ASSERT_EQ(reading.tell(), middle_on_pattern_edge);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte / 2);
}

TEST_F(ExtentsSuite, RollingWriteStrategyOneRollover) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_data_fk, OpenMode::Write);
    PatternHelper helper;
    auto total = helper.write(file, ((file.maximum_size() + 4096) / helper.size()));

    file.close();

    ASSERT_EQ(total, helper.bytes_written());

    auto skip = helper.size() - (file.truncated() - (file.truncated() / helper.size()) * helper.size());
    auto verified = helper.verify_file(layout, file_data_fk, skip);
    ASSERT_EQ(file.size(), verified + skip);
}

TEST_F(ExtentsSuite, RollingWriteStrategyTwoRollovers) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_data_fk, OpenMode::Write);

    PatternHelper helper;
    auto total = helper.write(file, ((file.maximum_size() * 2 + 4096) / helper.size()));

    file.close();

    ASSERT_EQ(total, helper.bytes_written());

    auto skip = helper.size() - (file.truncated() - (file.truncated() / helper.size()) * helper.size());
    auto verified = helper.verify_file(layout, file_data_fk, skip);
    ASSERT_EQ(file.size(), verified + skip);
}

TEST_F(ExtentsSuite, RollingWriteStrategyIndexWraparound) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_data_fk, OpenMode::Write);

    PatternHelper helper;
    auto total = helper.write(file, ((file.maximum_size() * 40) / helper.size()));

    file.close();

    ASSERT_EQ(total, helper.bytes_written());

    auto skip = helper.size() - (file.truncated() - (file.truncated() / helper.size()) * helper.size());
    auto verified = helper.verify_file(layout, file_data_fk, skip);
    ASSERT_EQ(file.size(), verified + skip);
}
