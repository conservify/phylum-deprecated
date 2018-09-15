#ifndef __PHYLUM_FILE_LAYOUT_H_INCLUDED
#define __PHYLUM_FILE_LAYOUT_H_INCLUDED

#include <cinttypes>

#include "phylum/private.h"
#include "phylum/file_index.h"
#include "phylum/file_allocation.h"
#include "phylum/file_table.h"
#include "phylum/file_descriptor.h"
#include "phylum/simple_file.h"
#include "phylum/file_preallocator.h"

namespace phylum {

struct FileStat {
    uint64_t size;
    uint32_t version;
};

class FileOpener {
public:
    virtual FileStat stat(FileDescriptor &fd) = 0;
    virtual SimpleFile open(FileDescriptor &fd, OpenMode mode) = 0;
    virtual bool erase(FileDescriptor &fd) = 0;

};

template<size_t SIZE>
class FileLayout : public FileOpener {
private:
    StorageBackend *storage_;
    FileDescriptor **fds_;
    FileAllocation allocations_[SIZE];

public:
    FileLayout(StorageBackend &storage) : storage_(&storage) {
    }

public:
    FileAllocation allocation(size_t i) const {
        return allocations_[i];
    }

    bool format(FileDescriptor*(&fds)[SIZE]) {
        FileTable table{ *storage_ };

        fds_ = fds;

        if (!allocate(fds)) {
            sdebug() << "Format allocation failed" << endl;
            return false;
        }

        if (!table.erase()) {
            sdebug() << "Format erase table failed" << endl;
            return false;
        }

        for (size_t i = 0; i < SIZE; ++i) {
            FileTableEntry entry;
            entry.magic.fill();
            memcpy(&entry.fd, fds_[i], sizeof(FileDescriptor));
            memcpy(&entry.alloc, &allocations_[i], sizeof(FileAllocation));
            if (!table.write(entry)) {
                sdebug() << "Format file write table failed: " << fds_[i]->name << endl;
                return false;
            }

            auto file = SimpleFile{
                storage_,
                fds_[i],
                &allocations_[i],
                i,
                OpenMode::Write
            };
            if (!file.format()) {
                sdebug() << "Format file failed: " << fds_[i]->name << endl;
                return false;
            }
        }

        return true;
    }

    bool mount(FileDescriptor*(&fds)[SIZE]) {
        FileTable table{ *storage_ };

        fds_ = fds;

        for (size_t i = 0; i < SIZE; ++i) {
            FileTableEntry entry;
            if (!table.read(entry)) {
                sdebug() << "Mounting error, table read failed: " << fds_[i]->name << endl;
                return false;
            }

            if (!entry.magic.valid()) {
                sdebug() << "Mounting error, table entry invalid: " << fds_[i]->name << endl;
                return false;
            }

            if (!entry.fd.compatible(fds[i])) {
                sdebug() << "Mounting error, table entry incompatible: " << fds_[i]->name << endl;
                return false;
            }

            memcpy(&allocations_[i], &entry.alloc, sizeof(FileAllocation));
        }

        return true;
    }

    bool unmount() {
        fds_ = nullptr;
        for (size_t i = 0; i < SIZE; ++i) {
            allocations_[i] = { };
        }

        return true;
    }

public:
    virtual FileStat stat(FileDescriptor &fd) override {
        auto file = open(fd, OpenMode::Read);
        if (!file) {
            return { 0, 0 };
        }
        auto size = file.size();
        auto version = file.version();
        file.close();
        return { size, version };
    }

    virtual SimpleFile open(FileDescriptor &fd, OpenMode mode = OpenMode::Read) override {
        for (size_t i = 0; i < SIZE; ++i) {
            if (fds_[i] == &fd) {
                auto file = SimpleFile{ storage_, fds_[i], &allocations_[i], i, mode };
                if (!file.initialize()) {
                    sdebug() << "Error initializing opening file." << endl;
                    break;
                }
                return file;
            }
        }
        return SimpleFile{ nullptr, nullptr, nullptr, 0, OpenMode::Read };
    }

    virtual bool erase(FileDescriptor &fd) override {
        for (size_t i = 0; i < SIZE; ++i) {
            if (fds_[i] == &fd) {
                auto file = SimpleFile{ storage_, fds_[i], &allocations_[i], i, OpenMode::Write };
                return file.erase();
            }
        }
        return true;
    }

private:
    bool allocate(FileDescriptor*(&fds)[SIZE]) {
        FilePreallocator allocator{ storage_->geometry() };

        for (size_t i = 0; i < SIZE; ++i) {
            if (!allocator.allocate((uint8_t)i, fds[i], allocations_[i])) {
                sdebug() << "Allocation failed: " << fds[i]->name << endl;
                return false;
            }
        }

        return true;
    }

};

}

#endif
