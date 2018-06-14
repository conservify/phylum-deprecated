#include <gtest/gtest.h>
#include <cstring>

#include "phylum/file_system.h"
#include "backends/linux_memory/linux_memory.h"

#include "utilities.h"

using namespace phylum;

static void write_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length,
                          int32_t total_to_write, int32_t &wrote);

static void read_and_verify_pattern(OpenFile &file, uint8_t *pattern,
                                    int32_t pattern_length, int32_t &read);

class FileOpsSuite : public ::testing::Test {
protected:
    Geometry geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    DebuggingBlockAllocator allocator_{ geometry_ };
    FileSystem fs_{ storage_, allocator_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(fs_.mount(true));
    }

    void TearDown() override {
        ASSERT_TRUE(fs_.unmount());
    }

};

TEST_F(FileOpsSuite, CreateFile) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    file.close();

    ASSERT_TRUE(fs_.exists("test.bin"));
}

TEST_F(FileOpsSuite, MountingFindsPreviousTree) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    file.close();

    ASSERT_TRUE(fs_.mount());

    ASSERT_TRUE(fs_.exists("test.bin"));
}

TEST_F(FileOpsSuite, WriteFile) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto file = fs_.open("test.bin");
    ASSERT_EQ(file.write("Jacob", 5), 5);
    ASSERT_EQ(file.size(), (uint32_t)5);
    file.close();
}

TEST_F(FileOpsSuite, WriteLessThanASectorAndRead) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write("Jacob", 5), 5);
    ASSERT_EQ(writing.size(), (uint32_t)5);
    writing.close();

    uint8_t buffer[32];
    auto reading = fs_.open("test.bin", true);
    ASSERT_EQ(reading.read(buffer, sizeof(buffer)), 5);
    reading.close();
}

TEST_F(FileOpsSuite, WriteTwoSectorsAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = SectorSize + SectorSize / 2;

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto read = 0, wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(wrote, total_writing);
    ASSERT_EQ(writing.size(), (uint32_t)total_writing);
    writing.close();

    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, WriteTwoBlocksAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() + SectorSize + SectorSize / 2);

    // This makes testing easier.
    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto read = 0, wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(wrote, total_writing);
    ASSERT_EQ(writing.size(), (uint32_t)total_writing);
    writing.close();

    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, WriteLessThanASectorAndAppendAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write(pattern, sizeof(pattern)), (int32_t)sizeof(pattern));
    writing.close();

    auto appending = fs_.open("test.bin");
    ASSERT_EQ(appending.write(pattern, sizeof(pattern)), (int32_t)sizeof(pattern));
    ASSERT_EQ(appending.size(), (uint32_t)sizeof(pattern) * 2);
    appending.close();

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, (int32_t)sizeof(pattern) * 2);
}

TEST_F(FileOpsSuite, WriteMultipleSectorsAndAppendAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = SectorSize + SectorSize / 2;

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    for (auto i = 0; i < 3; ++i) {
        auto writing = fs_.open("test.bin");
        write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
        writing.close();
    }

    ASSERT_EQ(wrote, (int32_t)total_writing * 3);

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, (int32_t)total_writing * 3);
}

TEST_F(FileOpsSuite, WriteTwoBlocksAndAppendAndRead) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() + SectorSize + SectorSize / 2);
    auto total_appending = SectorSize * 2;

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)total_writing);
    writing.close();

    auto appending = fs_.open("test.bin");
    write_pattern(appending, pattern, sizeof(pattern), total_appending, wrote);
    ASSERT_EQ(appending.size(), (uint32_t)(total_writing + total_appending));
    appending.close();

    ASSERT_EQ(wrote, total_writing + total_appending);

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, WriteToTailSectorAndAppend) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() + SectorSize);
    auto total_appending = SectorSize * 2;

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    auto appending = fs_.open("test.bin");
    write_pattern(appending, pattern, sizeof(pattern), total_appending, wrote);
    ASSERT_EQ(appending.size(), (uint32_t)(total_writing + total_appending));
    appending.close();

    ASSERT_EQ(wrote, total_writing + total_appending);

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, Write3BlocksAndReadAndSeekBeginning) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 3 + SectorSize);

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.seek(Seek::Beginning);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);
    reading.close();

    ASSERT_EQ(read, wrote * 2);
}

TEST_F(FileOpsSuite, Write128Blocks) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 128);

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    auto read = 0;
    auto reading = fs_.open("test.bin", true);
    read_and_verify_pattern(reading, pattern, sizeof(pattern), read);

    // Ensure that the size operation here doesn't need to read.
    storage_.log().clear();
    ASSERT_EQ(reading.size(), (uint32_t)(total_writing));
    ASSERT_EQ(storage_.log().size(), 0);

    reading.close();

    ASSERT_EQ(read, wrote);
}

TEST_F(FileOpsSuite, Write128BlocksAndCalculateLength) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 128);

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    // This requires a seek to the end of the file.
    auto reading = fs_.open("test.bin", true);
    storage_.log().clear();
    ASSERT_EQ(reading.size(), (uint32_t)(total_writing));
    ASSERT_EQ(storage_.log().size(), 11);
    reading.close();
}

TEST_F(FileOpsSuite, Write128BlocksAndSeekToEoF) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 128);

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    storage_.log().clear();

    auto reading = fs_.open("test.bin", true);
    ASSERT_EQ(reading.seek(Seek::End), (int32_t)total_writing);
    ASSERT_EQ(storage_.log().size(), 11);

    storage_.log().clear();
    ASSERT_EQ(reading.size(), (uint32_t)(total_writing));
    ASSERT_EQ(storage_.log().size(), 0);

    reading.close();
}

TEST_F(FileOpsSuite, Write128BlocksAndSeekToMiddle) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 128);

    ASSERT_EQ(total_writing % sizeof(pattern), (size_t)0);

    auto wrote = 0;
    auto writing = fs_.open("test.bin");
    write_pattern(writing, pattern, sizeof(pattern), total_writing, wrote);
    ASSERT_EQ(writing.size(), (uint32_t)(total_writing));
    writing.close();

    auto reading = fs_.open("test.bin", true);
    ASSERT_EQ(reading.seek(total_writing / 2), (int32_t)(total_writing / 2));

    ASSERT_EQ(reading.tell(), (uint32_t)(total_writing / 2));

    reading.close();
}

TEST_F(FileOpsSuite, MountingFindsPreviousTreeBlocks) {
    uint8_t pattern[] = { 'a', 's', 'd', 'f' };

    auto total_writing = (int32_t)(geometry_.block_size() * 512);

    auto wrote = 0;

    auto writing1 = fs_.open("test-1.bin");
    write_pattern(writing1, pattern, sizeof(pattern), total_writing, wrote);
    writing1.close();

    ASSERT_TRUE(fs_.exists("test-1.bin"));

    // BlockHelper helper1{ storage_, allocator_ };
    // helper1.dump(0, allocator_.state().head);

    DebuggingBlockAllocator second_allocator{ geometry_ };
    FileSystem second_fs{ storage_, second_allocator };

    ASSERT_TRUE(second_fs.mount());

    ASSERT_TRUE(second_fs.exists("test-1.bin"));

    for (auto name : { "test-2.bin", "test-3.bin", "test-4.bin" }) {
        auto writing = second_fs.open(name);
        write_pattern(writing, pattern, sizeof(pattern), geometry_.block_size(), wrote);
        writing.close();
    }

    ASSERT_TRUE(second_fs.exists("test-2.bin"));

    auto last_block = std::max(allocator_.state().head, second_allocator.state().head);
    BlockHelper helper{ storage_, second_allocator };
    // helper.dump(0, last_block);

    ASSERT_EQ(helper.number_of_chains(BlockType::Leaf, 0, last_block), 1);
    ASSERT_EQ(helper.number_of_chains(BlockType::Index, 0, last_block), 1);
    ASSERT_EQ(helper.number_of_chains(BlockType::File, 0, last_block), 4);

    ASSERT_GT(helper.number_of_blocks(BlockType::Leaf, 0, last_block), 1);
    ASSERT_GT(helper.number_of_blocks(BlockType::Index, 0, last_block), 1);
}

static void write_pattern(OpenFile &file, uint8_t *pattern, int32_t pattern_length,
                          int32_t total_to_write, int32_t &wrote) {
    auto written = 0;

    while (written < total_to_write) {
        if (file.write(pattern, pattern_length) != pattern_length) {
            break;
        }

        written += pattern_length;
        wrote += pattern_length;
    }
}

static void read_and_verify_pattern(OpenFile &file, uint8_t *pattern,
                                    int32_t pattern_length, int32_t &read) {
    uint8_t buffer[8];

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
