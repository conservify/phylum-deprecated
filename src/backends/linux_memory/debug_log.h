#ifndef __PHYLUM_LINUX_MEMORY_DEBUG_LOG_H_
#define __PHYLUM_LINUX_MEMORY_DEBUG_LOG_H_

#ifndef ARDUINO

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <cstdint>

#include <iostream>
#include <vector>
#include <list>

#include <phylum/phylum.h>
#include <phylum/private.h>

namespace phylum {

enum class OperationType {
    Opened,
    EraseBlock,
    Write,
    Read
};

class LogEntry {
private:
    OperationType type_;
    BlockAddress address_;
    uint8_t *ptr_;
    size_t size_;
    uint8_t *copy_;

public:
    LogEntry(OperationType type, uint8_t *ptr) :
        type_(type), ptr_(ptr), size_(0), copy_(nullptr) {
    }

    LogEntry(OperationType type, block_index_t block, uint8_t *ptr) :
        type_(type), address_(BlockAddress{ block, 0 }), ptr_(ptr), size_(0), copy_(nullptr) {
    }

    LogEntry(OperationType type, BlockAddress address, uint8_t *ptr, size_t size) :
        type_(type), address_(address), ptr_(ptr), size_(size), copy_(nullptr) {
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
    bool logging_{ false };

public:
    void append(LogEntry &&entry);

    void logging(bool logging) {
        logging_ = logging;
    }

    int32_t size() const {
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

#endif // ARDUINO

#endif // __PHYLUM_LINUX_MEMORY_DEBUG_LOG_H_
