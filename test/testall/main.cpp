#include <gtest/gtest.h>

#include <string>

#include "backends/linux_memory/linux_memory.h"

int main(int argc, char **argv) {
    auto include_memory_intesive = false;

    for (auto i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--erase-ff") {
            phylum::LinuxMemoryBackend::EraseByte = 0xff;
        }
        if (arg == "--erase-00") {
            phylum::LinuxMemoryBackend::EraseByte = 0x00;
        }
        if (arg == "--dev") {
            include_memory_intesive = true;
        }
    }

    ::testing::InitGoogleTest(&argc, argv);

    if (!include_memory_intesive) {
        ::testing::GTEST_FLAG(filter) = "-LargeDevices*";
    }

    return RUN_ALL_TESTS();
}
