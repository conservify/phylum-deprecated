#include "debug_log.h"

namespace phylum {

std::ostream& operator<<(std::ostream& os, const LogEntry& e) {
    switch (e.type_) {
    case OperationType::Opened:
        os << "Opened()";
        break;
    case OperationType::EraseBlock:
        os << "EraseBlock(" << e.address_ << ")";
        break;
    case OperationType::Read:
        os << "Read(" << e.address_ << " " << e.size_ << ")";
        break;
    case OperationType::Write:
        os << "Write(" << e.address_ << " " << e.size_ << ")";
        break;
    default:
        os << "Unknown";
        break;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const StorageLog& l) {
    for (auto &e : l.entries_) {
        os << e << std::endl;
    }
    return os;
}

}
