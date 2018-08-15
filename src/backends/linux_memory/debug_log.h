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
        free_backup();
    }

public:
    OperationType type() {
        return type_;
    }

    BlockAddress address() {
        return address_;
    }

    bool for_block(block_index_t block) {
        return address_.block == block;
    }

    void backup();

    void undo();

    void free_backup();

    bool can_undo() {
        return type_ == OperationType::Write || type_ == OperationType::EraseBlock;
    }

public:
    friend ostreamtype& operator<<(ostreamtype& os, const LogEntry& e);

};

class StorageLog {
private:
    bool copy_on_write_{ false };
    bool logging_{ false };
    std::list<LogEntry> entries_;

public:
    void append(LogEntry &&entry);

    void undo(size_t number);

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

    std::list<LogEntry> &entries() {
        return entries_;
    }

public:
    friend ostreamtype& operator<<(ostreamtype& os, const StorageLog& e);

};

}

#endif // ARDUINO

#endif // __PHYLUM_LINUX_MEMORY_DEBUG_LOG_H_
