#ifndef __PHYLUM_FILE_TABLE_H_INCLUDED
#define __PHYLUM_FILE_TABLE_H_INCLUDED

#include "phylum/file_descriptor.h"
#include "phylum/file_allocation.h"
#include "phylum/layout.h"

namespace phylum {

struct FileTableHead {
    BlockHead block;

    FileTableHead(BlockType type) : block(type) {
    }

    void fill() {
        block.fill();
    }

    bool valid() {
        return block.valid();
    }
};

struct FileTableEntry {
    BlockMagic magic;
    FileDescriptor fd;
    FileAllocation alloc;

    void fill() {
        magic.fill();
    }

    bool valid() {
        return magic.valid();
    }
};

inline ostreamtype& operator<<(ostreamtype& os, const FileTableEntry &e) {
    return os << "FileTableEntry<" << e.fd.name << " " << e.fd.maximum_size << " index=" << e.alloc.index << " data=" << e.alloc.data << ">";
}

struct FileTableTail {
    BlockTail block;
};

class FileTable {
private:
    BlockLayout<FileTableHead, FileTableTail> layout;

public:
    FileTable(StorageBackend &storage);

public:
    bool erase();
    bool write(FileTableEntry &entry);
    bool read(FileTableEntry &entry);

};

}

#endif
