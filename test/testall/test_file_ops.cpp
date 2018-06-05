#include <gtest/gtest.h>

#include "confs/file_system.h"
#include "backends/linux-memory/linux-memory.h"

#include "utilities.h"

using namespace confs;

class FileOpsSuite : public ::testing::Test {
protected:
    FileSystem<LinuxMemoryBackend> fs;

protected:
    void SetUp() override {
        ASSERT_TRUE(fs.initialize());
    }

    void TearDown() override {
        ASSERT_TRUE(fs.close());
    }

};

TEST_F(FileOpsSuite, CreateFile) {
    ASSERT_FALSE(fs.exists("test.bin"));

    auto file = fs.open("test.bin");
    // file.close();

    ASSERT_TRUE(fs.exists("test.bin"));
}

TEST_F(FileOpsSuite, WriteFile) {
}
