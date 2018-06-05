#include "utilities.h"

std::map<btree_key_t, btree_value_t> random_data() {
    std::map<btree_key_t, btree_value_t> data;

    for (auto i = 0; i < 32; ++i) {
        while (true) {
            auto raw = random() % 1024;
            if (raw > 0) {
                btree_key_t k = raw;
                data[k] = raw * 1024;
                break;
            }
        }
    }

    return data;
}

namespace confs {

std::ostream& operator<<(std::ostream& os, const btree_key_t& e) {
    os << e.data;
    return os;
}

}
