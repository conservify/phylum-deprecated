#ifndef __CONFS_LINUX_MEMORY_DEBUG_LOG_H_
#define __CONFS_LINUX_MEMORY_DEBUG_LOG_H_

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <iostream>
#include <vector>
#include <list>

#include <confs/confs.h>
#include <confs/private.h>

namespace confs {

enum class OperationType {
    Opened,
    EraseBlock,
    Write,
    Read
};

class LogEntry {
private:
    OperationType type_;
    block_index_t block_;
    sector_index_t sector_;
    uint8_t *ptr_;
    size_t size_;
    uint8_t *copy_;

public:
    LogEntry(OperationType type, uint8_t *ptr) :
        type_(type), block_(BLOCK_INDEX_INVALID), sector_(SECTOR_INDEX_INVALID), ptr_(ptr), size_(0), copy_(nullptr) {
    }

    LogEntry(OperationType type, block_index_t block, uint8_t *ptr) :
        type_(type), block_(block), sector_(SECTOR_INDEX_INVALID), ptr_(ptr), size_(0), copy_(nullptr) {
    }

    LogEntry(OperationType type, SectorAddress addr, uint8_t *ptr, size_t size) :
        type_(type), block_(addr.block), sector_(addr.sector), ptr_(ptr), size_(size), copy_(nullptr) {
    }

    virtual ~LogEntry() {
        if (copy_ != nullptr) {
            free(copy_);
            copy_ = nullptr;
        }
    }

public:
    OperationType type() {
        return type_;
    }

    void backup() {
        assert(copy_ == nullptr);

        if (ptr_ != nullptr) {
            copy_ = (uint8_t *)malloc(size_);
            memcpy(copy_, ptr_, size_);
        }
    }

    void undo() {
        assert(copy_ != nullptr);

        if (ptr_ != nullptr) {
            memcpy(ptr_, copy_, size_);
        }
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const LogEntry& e);

};

class StorageLog {
private:
    bool copy_on_write_{ false };
    std::list<LogEntry> entries_;

public:
    void append(LogEntry &&entry) {
        entries_.emplace_back(std::move(entry));
        if (copy_on_write_) {
            entries_.back().backup();
        }

        // sdebug << entry << std::endl;
    }

    uint32_t size() const {
        return entries_.size();
    }

    void clear() {
        entries_.erase(entries_.begin(), entries_.end());
    }

    void copy_on_write(bool enabled) {
        copy_on_write_ = enabled;
    }

public:
    friend std::ostream& operator<<(std::ostream& os, const StorageLog& e);

};

}

#endif
