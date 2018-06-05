#include <gtest/gtest.h>

#include "utilities.h"
#include "confs/super_block.h"
#include "backends/linux-memory/linux-memory.h"

using namespace confs;

class FileOpsSuite : public ::testing::Test {
protected:
    confs_geometry_t geometry_{ 1024, 4, 4, 512 };
    LinuxMemoryBackend storage_;
    BlockAllocator allocator_{ storage_ };
    SuperBlockManager sbm_{ storage_, allocator_ };

protected:
    void SetUp() override {
        ASSERT_TRUE(storage_.initialize(geometry_));
        ASSERT_TRUE(storage_.open());
        ASSERT_TRUE(sbm_.create());
        ASSERT_TRUE(sbm_.locate());
    }

    void TearDown() override {
        ASSERT_TRUE(storage_.close());
    }

};

TEST_F(FileOpsSuite, Mount) {
}

