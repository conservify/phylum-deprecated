#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "phylum/preallocated.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

class PreallocatedSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
    }

};

TEST_F(PreallocatedSuite, FormattingStandardLayout) {
    FileDescriptor file_system_area_fd =   { "system",        100 };
    FileDescriptor file_log_startup_fd =   { "startup.log",   100 };
    FileDescriptor file_log_now_fd =       { "now.log",       100 };
    FileDescriptor file_log_emergency_fd = { "emergency.log", 100 };
    FileDescriptor file_data_fd =          { "data.fk",       0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
        &file_log_now_fd,
        &file_log_emergency_fd,
        &file_data_fd
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

TEST_F(PreallocatedSuite, WritingSmallFileToItsEnd) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_TRUE(file);

    ASSERT_EQ(file.version(), (uint32_t)1);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());
    file.close();

    ASSERT_EQ(total, file.maximum_size());

    auto verified = helper.verify_file(layout, data_file);
    ASSERT_EQ(total, verified);
}

TEST_F(PreallocatedSuite, WritingAndThenErasing) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());
    ASSERT_EQ(file.size(), file.maximum_size());
    file.close();

    ASSERT_EQ(file.maximum_size(), total);
    ASSERT_EQ(file.version(), (uint32_t)1);

    auto verified = helper.verify_file(layout, data_file);
    ASSERT_EQ(total, verified);

    ASSERT_TRUE(layout.erase(data_file));

    auto file2 = layout.open(data_file, OpenMode::Write);
    ASSERT_TRUE(file2);
    ASSERT_EQ(file2.version(), (uint32_t)2);
    ASSERT_EQ(file2.size(), (uint64_t)0);
    ASSERT_EQ(file2.tell(), (uint64_t)0);
}

TEST_F(PreallocatedSuite, ErasingTwice) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };
    PatternHelper helper;

    ASSERT_TRUE(layout.format(files));

    auto file1 = layout.open(data_file, OpenMode::Write);
    helper.write(file1, 1024 / helper.size());
    ASSERT_EQ(file1.version(), (uint32_t)1);
    file1.close();

    ASSERT_TRUE(layout.erase(data_file));

    auto file2 = layout.open(data_file, OpenMode::Write);
    helper.write(file2, 1024 / helper.size());
    ASSERT_EQ(file2.version(), (uint32_t)2);
    file2.close();

    ASSERT_TRUE(layout.erase(data_file));

    auto file3 = layout.open(data_file, OpenMode::Write);
    helper.write(file3, 1024 / helper.size());
    ASSERT_EQ(file3.version(), (uint32_t)3);
    file3.close();
}

TEST_F(PreallocatedSuite, FormattingLeavesVersions) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };
    PatternHelper helper;

    ASSERT_TRUE(layout.format(files));

    auto file1 = layout.open(data_file, OpenMode::Write);
    helper.write(file1, 1024 / helper.size());
    ASSERT_EQ(file1.version(), (uint32_t)1);
    file1.close();

    ASSERT_TRUE(layout.format(files));

    auto file2 = layout.open(data_file, OpenMode::Write);
    helper.write(file2, 1024 / helper.size());
    ASSERT_EQ(file2.version(), (uint32_t)1);
    file2.close();
}

TEST_F(PreallocatedSuite, WriteBlockInTwoSeparateOpensWritesCorrectBytesInBlock) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    PatternHelper helper;

    auto ideal_block_size = SectorSize * 16;
    auto file1 = layout.open(data_file, OpenMode::Write);
    helper.write(file1, ideal_block_size / 2 / helper.size());
    file1.close();

    auto file2 = layout.open(data_file, OpenMode::Write);
    helper.write(file2, ideal_block_size / 2 / helper.size());
    file2.close();

    auto verified = helper.verify_file(layout, data_file);

    ASSERT_EQ(verified, (uint64_t)ideal_block_size);

    auto file = layout.open(data_file, OpenMode::Read);
    ASSERT_EQ(verified, file.size());
}

TEST_F(PreallocatedSuite, WritingOneMegabyteToFile) {
    FileDescriptor data_file = { "data.fk", 0 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_TRUE(file);

    PatternHelper helper;
    auto total = helper.write(file, (1024 * 1024) / helper.size());

    file.close();

    ASSERT_EQ(total, (uint64_t)1024 * 1024);

    auto verified = helper.verify_file(layout, data_file);
    ASSERT_EQ(total, verified);
}

TEST_F(PreallocatedSuite, AppendingOneMegabyteTwoOneMegabyte) {
    FileDescriptor data_file = { "data.fk", 0 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;

    PatternHelper helper;
    {
        auto file = layout.open(data_file, OpenMode::Write);
        ASSERT_TRUE(file);
        ASSERT_EQ(file.size(), (uint64_t)0);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    {
        auto file = layout.open(data_file, OpenMode::Write);
        ASSERT_TRUE(file);
        ASSERT_TRUE(file.seek(UINT64_MAX));
        ASSERT_EQ(file.size(), OneMegabyte);

        auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());

        file.close();

        ASSERT_EQ(file.size(), 2 * OneMegabyte);
        ASSERT_EQ(total, OneMegabyte);
    }

    auto verified = helper.verify_file(layout, data_file);
    ASSERT_EQ(OneMegabyte * 2, verified);
}

TEST_F(PreallocatedSuite, SeekingEndCalculatesFileSize) {
    FileDescriptor data_file = { "data.fk", 0 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;
    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_EQ(file.size(), (uint64_t)0);
    PatternHelper helper;
    auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());
    file.close();

    ASSERT_EQ(file.size(), OneMegabyte);
    ASSERT_EQ(total, OneMegabyte);

    auto reading = layout.open(data_file);
    ASSERT_EQ(reading.size(), (uint64_t)OneMegabyte);
    reading.seek(0);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte);
}

TEST_F(PreallocatedSuite, SeekingInFileWithUnwrittenTailBlock) {
    FileDescriptor data_file = { "data.fk", 0 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(data_file, OpenMode::Write);
    PatternHelper helper;
    while (true) {
        helper.write(file, 1);

        auto addr = file.head();
        addr.add(SectorSize);
        if (addr.tail_sector(geometry_)) {
            break;
        }
    }
    file.close();

    auto reading = layout.open(data_file);
    reading.seek(UINT64_MAX);
}

TEST_F(PreallocatedSuite, SeekMiddleOfFile) {
    FileDescriptor data_file = { "data.fk", 0 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    constexpr uint64_t OneMegabyte = 1024 * 1024;
    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_EQ(file.size(), (uint64_t)0);
    PatternHelper helper;
    auto total = helper.write(file, (int32_t)OneMegabyte / helper.size());
    file.close();

    ASSERT_EQ(file.size(), OneMegabyte);
    ASSERT_EQ(total, OneMegabyte);

    auto middle_on_pattern_edge = ((OneMegabyte / 2) / helper.size()) * helper.size();
    auto reading = layout.open(data_file);
    ASSERT_EQ(reading.size(), (uint64_t)OneMegabyte);
    ASSERT_TRUE(reading.seek(middle_on_pattern_edge));
    ASSERT_EQ(reading.tell(), middle_on_pattern_edge);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte / 2);

    ASSERT_TRUE(reading.seek(0));
    ASSERT_EQ(reading.tell(), (uint64_t)0);
}

struct TestStruct {
    uint32_t time;
    uint64_t position;
};

TEST_F(PreallocatedSuite, WriteAFewBlocksAndReadLastBlock) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    for (uint32_t i = 0; i < 4; ++i) {
        TestStruct test = { i, i };
        auto writing = layout.open(data_file, OpenMode::Write);
        writing.write((uint8_t *)&test, sizeof(TestStruct));
        ASSERT_EQ(sizeof(TestStruct) * (i + 1), writing.size());
        writing.close();
    }

    TestStruct test = { 0, 0 };
    auto reading = layout.open(data_file, OpenMode::Read);

    reading.seek(sizeof(TestStruct) * 3);
    ASSERT_EQ(sizeof(TestStruct) * 3, reading.tell());
    ASSERT_EQ(reading.read((uint8_t *)&test, sizeof(TestStruct)), (int32_t)sizeof(TestStruct));
    ASSERT_EQ(sizeof(TestStruct) * 4, reading.tell());
    ASSERT_TRUE(reading.seek(0));
    ASSERT_EQ(reading.tell(), (uint64_t)0);

    reading.close();
}

TEST_F(PreallocatedSuite, ResilienceIndexWriteFails) {
    FileDescriptor data_file = { "data.fk", 100 };
    static FileDescriptor* files[] = { &data_file };
    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    storage_.log().copy_on_write(true);
    // Makes finding the mid file index write easier, drops the initial format from the log.
    storage_.log().clear();

    auto file = layout.open(data_file, OpenMode::Write);
    ASSERT_TRUE(file);

    PatternHelper helper;
    helper.write(file, (70 * 1024) / helper.size());
    file.close();

    auto f = [&](LogEntry &e) -> bool {
                 if (e.type() == OperationType::Write) {
                     if (e.for_block(file.allocation().index.start)) {
                         return true;
                     }
                 }
                 return false;
    };

    ASSERT_GT(undo_everything_after(storage_, f), 1);

    auto verified = helper.verify_file(layout, data_file);
    ASSERT_EQ((uint32_t)60896, verified);

    file = layout.open(data_file, OpenMode::Write);
    helper.write(file, (70 * 1024 - 60896) / helper.size());
    file.close();

    // This is a little larger than 70 * 1024 because the file was truncated in
    // the middle of a pattern, so it's 64 bytes more.
    ASSERT_EQ((uint32_t)(71648), file.size());

    // This will yield an index record on the block after the one we failed to
    // write. Which, I don't think is a huge deal.
}

