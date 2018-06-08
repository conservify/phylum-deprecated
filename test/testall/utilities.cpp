#include "utilities.h"

std::map<uint64_t, uint64_t> random_data() {
    std::map<uint64_t, uint64_t> data;

    for (auto i = 0; i < 32; ++i) {
        while (true) {
            auto raw = random() % 1024;
            if (raw > 0) {
                data[raw] = raw * 1024;
                break;
            }
        }
    }

    return data;
}
