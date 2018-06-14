#ifndef __PHYLUM_FILE_SYSTEM_H_INCLUDED
#define __PHYLUM_FILE_SYSTEM_H_INCLUDED

#include "phylum/persisted_tree.h"
#include "phylum/block_alloc.h"
#include "phylum/super_block.h"
#include "phylum/crc.h"
#include "phylum/inodes.h"
#include "phylum/backend_nodes.h"
#include "phylum/journal.h"
#include "phylum/free_pile.h"

namespace phylum {

class FileSystem;

enum class Seek {
    Beginning,
    End,
};

class OpenFile {
    static constexpr uint32_t InvalidLengthOrPosition = UINT32_MAX;
    static constexpr uint8_t PositionSaveFrequency = 8;
    static constexpr int32_t SeekFailed = INT32_MAX;

private:
    FileSystem *fs_;
    uint32_t id_;
    BlockAddress head_;
    bool readonly_{ false };

    uint32_t bytes_in_block_{ 0 };
    uint32_t length_{ 0 };
    uint32_t position_{ 0 };
    uint8_t blocks_since_save_{ 0 };

    uint8_t buffer_[SectorSize];
    uint16_t available_{ 0 };
    uint16_t buffpos_{ 0 };

public:
    OpenFile(FileSystem &fs, file_id_t id, bool readonly);

public:
    bool open();
    bool open_or_create();
    uint32_t size();
    uint32_t tell();
    int32_t seek(Seek seek, uint32_t position = 0);
    int32_t seek(uint32_t position);
    int32_t write(const void *ptr, size_t size);
    int32_t read(void *ptr, size_t size);
    void close();

    operator bool() {
        return open();
    }

private:
    int32_t flush();
    bool tail_sector();

    struct SeekStatistics {
        BlockAddress address;
        int32_t blocks;
        int32_t bytes;
    };

    SeekStatistics seek(BlockAddress starting, uint32_t max);
    BlockAddress initialize_block(block_index_t block, block_index_t previous);

};

class FileSystem {
private:
    using NodeType = Node<uint64_t, uint64_t, BlockAddress, 6, 6>;

    StorageBackend *storage_;
    BlockAllocator *allocator_;
    SuperBlockManager sbm_;
    StorageBackendNodeStorage<NodeType> nodes_;
    BlockAddress tree_addr_;
    Journal journal_;
    FreePileManager fpm_;

public:
    FileSystem(StorageBackend &storage, BlockAllocator &allocator);

    template<typename NodeType>
    friend struct TreeContext;
    friend class OpenFile;

public:
    StorageBackend &storage() {
        return *storage_;
    }

    SuperBlock &sb() {
        return sbm_.block();
    }

    Journal &journal() {
        return journal_;
    }

    FreePileManager &fpm() {
        return fpm_;
    }

public:
    bool mount(bool wipe = false);
    bool exists(const char *name);
    OpenFile open(const char *name, bool readonly = false);
    bool gc();
    bool unmount();

private:
    bool touch();
    bool format();
    void prepare(SuperBlock &sb);

};

}

#endif
