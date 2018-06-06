#include <gtest/gtest.h>

#include "confs/file_system.h"
#include "backends/linux-memory/linux-memory.h"

#include "utilities.h"

using namespace confs;

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

TEST_F(FileOpsSuite, WriteAndReadFile) {
    ASSERT_FALSE(fs_.exists("test.bin"));

    auto writing = fs_.open("test.bin");
    ASSERT_EQ(writing.write("Jacob", 5), 5);
    writing.close();

    uint8_t buffer[512];
    auto reading = fs_.open("test.bin", true);
    ASSERT_EQ(reading.read(buffer, sizeof(buffer)), 5);
    reading.close();
}
