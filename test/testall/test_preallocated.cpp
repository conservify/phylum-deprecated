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

    ASSERT_EQ(verified, (uint64_t)ideal_block_size);

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

struct TestStruct {
    uint32_t time;
    uint64_t position;
};

TEST_F(PreallocatedSuite, WriteAFewBlocksAndReadLastBlock) {
    FileDescriptor file_log_startup_fd = { "startup.log", WriteStrategy::Append, 100 };

    static FileDescriptor* files[] = {
        &file_log_startup_fd,
    };

    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    for (uint32_t i = 0; i < 4; ++i) {
        TestStruct test = { i, i };
        auto writing = layout.open(file_log_startup_fd, OpenMode::Write);
        writing.write((uint8_t *)&test, sizeof(TestStruct));
        ASSERT_EQ(sizeof(TestStruct) * (i + 1), writing.size());
        writing.close();
    }

    TestStruct test = { 0, 0 };
    auto reading = layout.open(file_log_startup_fd, OpenMode::Read);

    reading.seek(sizeof(TestStruct) * 3);
    ASSERT_EQ(sizeof(TestStruct) * 3, reading.tell());
    ASSERT_EQ(reading.read((uint8_t *)&test, sizeof(TestStruct)), (int32_t)sizeof(TestStruct));
    ASSERT_EQ(sizeof(TestStruct) * 4, reading.tell());
    reading.close();
}

template<typename Predicate>
inline static int32_t undo_everything_after(LinuxMemoryBackend &storage, Predicate predicate, bool log = false) {
    auto c = 0;
    auto seen = false;
    for (auto &l : storage.log().entries()) {
        if (predicate(l)) {
            seen = true;
        }
        if (seen && l.can_undo()) {
            if (log) {
                sdebug() << "Undo: " << l << endl;
            }
            l.undo();
            c++;
        }
    }
    return c;
}

TEST_F(PreallocatedSuite, ResilienceIndexWriteFails) {
    FileDescriptor file_log_startup_fd = { "startup.log", WriteStrategy::Append, 100 };

    static FileDescriptor* files[] = {
        &file_log_startup_fd,
    };

    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    storage_.log().copy_on_write(true);
    // Makes finding the mid file index write easier, drops the initial format from the log.
    storage_.log().clear();

    auto file = layout.open(file_log_startup_fd, OpenMode::Write);
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

    auto verified = helper.verify_file(layout, file_log_startup_fd);
    ASSERT_EQ((uint32_t)61120, verified);

    file = layout.open(file_log_startup_fd, OpenMode::Write);
    helper.write(file, (70 * 1024 - 61120) / helper.size());
    file.close();

    // This is a little larger than 70 * 1024 because the file was truncated in
    // the middle of a pattern, so it's 64 bytes more.
    ASSERT_EQ((uint32_t)(71616), file.size());

    // This will yield an index record on the block after the one we failed to
    // write. Which, I don't think is a huge deal.
}

TEST_F(PreallocatedSuite, DISABLED_ResilienceReindexWriteFails) {
    FileDescriptor file_log_startup_fd = { "startup.log", WriteStrategy::Rolling, 100 };

    static FileDescriptor* files[] = {
        &file_log_startup_fd,
    };

    FileLayout<1> layout{ storage_ };

    ASSERT_TRUE(layout.format(files));

    // storage_.log().logging(true);

    storage_.log().copy_on_write(true);
    // Makes finding the mid file index write easier, drops the initial format from the log.
    storage_.log().clear();

    {
        auto file = layout.open(file_log_startup_fd, OpenMode::Write);
        PatternHelper helper;
        helper.write(file, (110 * 1024) / helper.size());
        file.close();

        auto f = [&](LogEntry &e) -> bool {
                    if (e.type() == OperationType::Write) {
                        auto block = file.allocation().index.start;
                        BlockAddress reindex_entry{ block, 96 + SectorSize };
                        if (e.address() == reindex_entry) {
                            return true;
                        }
                    }
                    return false;
        };

        ASSERT_GT(undo_everything_after(storage_, f, true), 1);
    }

    {
        auto file = layout.open(file_log_startup_fd, OpenMode::Write);
        ASSERT_EQ(file.size(), file.maximum_size());
    }
}
