#include "phylum/file_table.h"

namespace phylum {

FileTable::FileTable(StorageBackend &storage) :
    layout{ storage, empty_allocator, { 0, 0 }, BlockType::Index } {
}

bool FileTable::erase() {
    if (!layout.write_head(0)) {
        return false;
    }

    return true;
}

bool FileTable::write(FileTableEntry &entry) {
    if (!layout.append(entry)) {
        return false;
    }

    return true;
}

bool FileTable::read(FileTableEntry &entry) {
    if (!layout.walk(entry)) {
        return false;
    }

    return true;
}

}
