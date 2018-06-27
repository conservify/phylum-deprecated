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

    void TearDown() override {
    }

};

TEST_F(PreallocatedSuite, StandardLayoutAllocating) {
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

TEST_F(PreallocatedSuite, SmallFileWritingToEnd) {
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

TEST_F(PreallocatedSuite, WriteBlockInTwoSeparateOpensWritesCorrectBytesInBlock) {
    FileDescriptor file_system_area_fd = { "system",      WriteStrategy::Append,  100 };
    FileDescriptor file_log_startup_fd = { "startup.log", WriteStrategy::Append,  100 };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    PatternHelper helper;

    auto ideal_block_size = SectorSize * 16;
    auto file1 = layout.open(file_log_startup_fd, OpenMode::Write);
    helper.write(file1, ideal_block_size / 2 / helper.size());
    file1.close();

    auto file2 = layout.open(file_log_startup_fd, OpenMode::Write);
    helper.write(file2, ideal_block_size / 2 / helper.size());
    file2.close();

    auto verified = helper.verify_file(layout, file_log_startup_fd);

    ASSERT_EQ(verified, ideal_block_size);

    auto file = layout.open(file_log_startup_fd, OpenMode::Read);
    ASSERT_EQ(verified, file.size());
}

TEST_F(PreallocatedSuite, LargeFileWritingToEnd) {
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

TEST_F(PreallocatedSuite, LargeFileAppending) {
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

TEST_F(PreallocatedSuite, SeekingEndCalculatesFileSize) {
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
    ASSERT_EQ(reading.size(), (uint64_t)OneMegabyte);
    reading.seek(0);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte);
}

TEST_F(PreallocatedSuite, SeekingToFileWithUnwrittenTailBlock) {
    FileDescriptor file_system_area_fd =   { "system",  WriteStrategy::Append,  100 };
    FileDescriptor file_data_fk =          { "data.fk", WriteStrategy::Append,  0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_data_fk
    };

    FileLayout<2> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    auto file = layout.open(file_data_fk, OpenMode::Write);
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

    auto reading = layout.open(file_data_fk);
    reading.seek(UINT64_MAX);
}

TEST_F(PreallocatedSuite, SeekMiddleOfFile) {
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
    ASSERT_EQ(reading.size(), (uint64_t)OneMegabyte);
    ASSERT_TRUE(reading.seek(middle_on_pattern_edge));
    ASSERT_EQ(reading.tell(), middle_on_pattern_edge);
    auto verified = helper.read(reading);
    reading.close();

    ASSERT_EQ(verified, OneMegabyte / 2);
}

TEST_F(PreallocatedSuite, RollingWriteStrategyOneRollover) {
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

TEST_F(PreallocatedSuite, RollingWriteStrategyTwoRollovers) {
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

TEST_F(PreallocatedSuite, RollingWriteStrategyIndexWraparound) {
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

class FileIndexSuite : public ::testing::Test {
protected:

protected:
    void SetUp() override {
    }

    void TearDown() override {
    }

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

    ASSERT_EQ(helper.number_of_blocks(BlockType::Index, 0, blocks_required - 1), 3329);
}
