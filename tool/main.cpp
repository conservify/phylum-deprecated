#include <cinttypes>
#include <cassert>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <phylum/tree_fs_super_block.h>
#include <phylum/files.h>
#include <phylum/unused_block_reclaimer.h>
#include <phylum/basic_super_block_manager.h>
#include <backends/linux_memory/linux_memory.h>

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;

uint64_t get_file_size(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

static size_t log_message_hook(const LogMessage *m, const char *formatted, void *arg) {
    return 0;
}

class BlockLogger : public phylum::BlockVisitor {
public:
    BlockLogger() {
    }

public:
    void block(phylum::block_index_t block) override {
        Log::info("Block: %d", block);
    }

    bool is_free(phylum::block_index_t block) {
        return false;
    }

};

int32_t main(int32_t argc, const char **argv) {
    log_configure_hook_register(log_message_hook, nullptr);
    log_configure_time(nullptr, nullptr);

    if (argc == 1) {
        return 2;
    }

    auto file_name = argv[1];
    auto file_size = get_file_size(file_name);

    Log::info("Starting: %ld (%s)", file_size, file_name);

    auto fd = open(file_name, O_RDONLY, 0);
    assert(fd != -1);

    auto ptr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    assert(ptr != MAP_FAILED);

    auto number_of_blocks = file_size / (uint64_t)(phylum::SectorSize * 4 * 4);

    Log::info("Starting: %ld (%d)", number_of_blocks, phylum::SectorSize);

    phylum::Geometry geometry{ number_of_blocks, 4, 4, phylum::SectorSize };
    phylum::LinuxMemoryBackend storage;
    phylum::FileLayout<5> fs{ storage };
    phylum::FileDescriptor file_system_area_fd = { "system",          100  };
    phylum::FileDescriptor file_emergency_fd   = { "emergency.fklog", 100  };
    phylum::FileDescriptor file_logs_a_fd =      { "logs-a.fklog",    2048 };
    phylum::FileDescriptor file_logs_b_fd =      { "logs-b.fklog",    2048 };
    phylum::FileDescriptor file_data_fk =        { "data.fk",         0    };
    phylum::FileDescriptor* descriptors[5]{
      &file_system_area_fd,
      &file_emergency_fd,
      &file_logs_a_fd,
      &file_logs_b_fd,
      &file_data_fk
    };

    assert(storage.open(ptr, geometry));

    phylum::phylog().errors() << "G " << geometry << endl;

    Log::info("Mounting...");

    if (!fs.mount(descriptors)) {
        Log::error("Mounting failed!");
    }

    Log::info("Mounted (%d blocks)", storage.geometry().number_of_blocks);

    phylum::SerialFlashAllocator allocator{ storage };

    auto data_alloc = fs.allocation(4);
    auto head = data_alloc.data.beginning();

    // phylum::BlockedFile bf{ &storage, 0, phylum::OpenMode::Read, head };
    phylum::AllocatedBlockedFile file{ &storage, phylum::OpenMode::Read, &allocator, head };

    BlockLogger block_logger;

    file.walk(&block_logger);

    /*
    for (auto fd : descriptors) {
        Log::info("Opening: %s", fd->name);

        auto opened = fs.open(*fd);
        if (opened) {
            Log::info("File: %s size = %d maximum = %d", fd->name, (uint32_t)opened.size(),
                      (uint32_t)fd->maximum_size);
        }
    }
    */

    assert(munmap(ptr, file_size) == 0);
    close(fd);

    return 0;
}
