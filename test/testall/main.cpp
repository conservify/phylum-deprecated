#include <gtest/gtest.h>

#include <string>

#include "backends/linux_memory/linux_memory.h"

int main(int argc, char **argv) {
    for (auto i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--erase-ff") {
            phylum::LinuxMemoryBackend::EraseByte = 0xff;
        }
        if (arg == "--erase-00") {
            phylum::LinuxMemoryBackend::EraseByte = 0x00;
        }
    }

    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
