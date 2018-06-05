#include <gtest/gtest.h>

class FileOpsSuite : public ::testing::Test {
protected:
    FileOpsSuite();
    virtual ~FileOpsSuite();

    virtual void SetUp();
    virtual void TearDown();

};
