#include <gtest/gtest.h>
#include <cstring>

#include "confs/file_system.h"
#include "backends/linux-memory/linux-memory.h"

#include "utilities.h"

using namespace confs;

static void write_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length, int32_t total_to_write, int32_t &wrote);
static void read_and_verify_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length, int32_t &read);

class FileOpsSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    FileSystem fs_{ storage_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(fs_.initialize(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.close());
    }

};

TEST_F(FileOpsSuite, CreateFile) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    file.close();

    ASSERT_TRUE(fs_.exists("test.bin"));
}

TEST_F(FileOpsSuite, InitializeFindsPreviousTree) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    file.close();

    ASSERT_TRUE(fs_.open());

    ASSERT_TRUE(fs_.exists("test.bin"));
}

TEST_F(FileOpsSuite, WriteFile) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    ASSERT_EQ(file.write("Jacob", 5), 5);
    file.close();
}

TEST_F(FileOpsSuite, WriteLessThanASectorAndRead) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write("Jacob", 5), 5);
    writing.close();

    uint8_t buffer[32];
    auto reading = fs_.open("test.bin", true);
    ASSERT_EQ(reading.read(buffer, sizeof(buffer)), 5);
    reading.close();
}

TEST_F(FileOpsSuite, WriteLessThanASectorAndAppendAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write(pattern, sizeof(pattern)), (int32_t)sizeof(pattern));
    writing.close();

    writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write(pattern, sizeof(pattern)), (int32_t)sizeof(pattern));
    writing.close();

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, (int32_t)sizeof(pattern) * 2);
}

TEST_F(FileOpsSuite, WriteAndReadTwoSectors) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = SectorSize + SectorSize / 2;

    // This makes testing easier.
    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto read = 0, wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(wrote, total_writing);
    writing.close();

    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, WriteAndReadIntoSecondBlock) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() + SectorSize + SectorSize / 2);

    // This makes testing easier.
    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto read = 0, wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(wrote, total_writing);
    writing.close();

    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

static void write_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length, int32_t total_to_write, int32_t &wrote) {
    wrote = 0;

    while (wrote < total_to_write) {
        if (file.write(pattern, pattern_length) != pattern_length) {
            break;
        }

        wrote += pattern_length;
    }
}

static void read_and_verify_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length, int32_t &read) {
    uint8_t buffer[8];

    read = 0;

    ASSERT_EQ(sizeof(buffer) % pattern_length, (size_t) 0);

    while (true) {
        auto bytes = file.read(buffer, sizeof(buffer));
        if (bytes == 0) {
            break;
        }

        auto i = 0;
        while (i < bytes) {
            auto left = bytes - i;
            auto pattern_position = read % pattern_length;
            auto comparing = left > (pattern_length - pattern_position) ? (pattern_length - pattern_position) : left;

            ASSERT_EQ(memcmp(buffer + i, pattern + pattern_position, comparing), 0);

            i += comparing;
            read += comparing;
        }
    }
}
