#include <cinttypes>
#include <cassert>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

#include <string>
#include <experimental/filesystem>

#include <phylum/tree_fs_super_block.h>
#include <phylum/files.h>
#include <phylum/unused_block_reclaimer.h>
#include <phylum/basic_super_block_manager.h>
#include <backends/linux_memory/linux_memory.h>

#include "phylum_input_stream.h"
#include "record_walker.h"

namespace fs = std::experimental::filesystem;

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;

using namespace phylum;

uint64_t get_file_size(const char* filename) {
    struct stat st;
    stat(filename, &st);
    return st.st_size;
}

static size_t log_message_hook(const LogMessage *m, const char *formatted, void *arg) {
    return 0;
}

struct Args {
    fs::path image;
    fs::path directory;
    bool log{ false };

    bool is_file(std::string s) {
        std::ifstream f(s.c_str());
        return f.good();
    }

    bool parse(int32_t argc, const char **argv) {
        std::error_code ec;
        auto good = false;

        for (auto i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (fs::is_regular_file(arg, ec)) {
                image = arg;
                good = true;
            }

            if (fs::is_directory(arg, ec)) {
                directory = arg;
            }

            if (arg == "--log") {
                log = true;
            }
        }

        return good;
    }
};

int32_t main(int32_t argc, const char **argv) {
    log_configure_hook_register(log_message_hook, nullptr);
    log_configure_time(nullptr, nullptr);

    Args args;
    if (!args.parse(argc, argv)) {
        return 2;
    }

    auto file_name = args.image.c_str();
    auto file_size = get_file_size(file_name);

    Log::info("Opening %s", file_name);

    auto fd = open(file_name, O_RDONLY, 0);
    assert(fd != -1);

    Log::info("Mapping");

    auto ptr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE /* | MAP_POPULATE */, fd, 0);
    assert(ptr != MAP_FAILED);

    Log::info("Mounting");

    auto number_of_blocks = file_size / (uint64_t)(SectorSize * 4 * 4);

    Geometry geometry{ (block_index_t)number_of_blocks, 4, 4, SectorSize };
    LinuxMemoryBackend storage;
    FileLayout<5> fs{ storage };
    FileDescriptor file_system_area_fd = { "system",          100  };
    FileDescriptor file_emergency_fd   = { "emergency.fklog", 100  };
    FileDescriptor file_logs_a_fd =      { "logs-a.fklog",    2048 };
    FileDescriptor file_logs_b_fd =      { "logs-b.fklog",    2048 };
    FileDescriptor file_data_fk =        { "data.fk",         0    };
    FileDescriptor* descriptors[5]{
      &file_system_area_fd,
      &file_emergency_fd,
      &file_logs_a_fd,
      &file_logs_b_fd,
      &file_data_fk
    };

    assert(storage.open(ptr, geometry));

    if (!fs.mount(descriptors)) {
        Log::error("Mounting failed!");
        return 2;
    }

    for (auto fd : descriptors) {
        auto opened = fs.open(*fd);
        if (opened) {
            Log::info("File: %s size = %d", fd->name, (uint32_t)opened.size());

            if (!opened.seek(0)) {
                Log::error("Error seeking to beginning of file!");
                return 2;
            }

            if (!args.log) {
                RecordWalker walker("/home/jlewallen/fieldkit/data-protocol/fk-data.proto", "DataRecord");
                LoggingVisitor visitor;
                PhylumInputStream stream{ geometry, reinterpret_cast<uint8_t*>(ptr), opened.head().block };

                if (!walker.walk(stream, visitor)) {
                    return false;
                }
                if (stream.position() > 0) {
                    sdebug() << "Done: " << stream.position() << endl;
                }
            }

            if (!args.directory.empty() && opened.size() > 0) {
                auto path = args.directory / fs::path{ fd->name };
                auto total = 0;

                auto fp = fopen(path.c_str(), "wb");
                if (fp == nullptr) {
                    return 2;
                }

                while (true) {
                    uint8_t buffer[512];

                    auto read = opened.read(buffer, sizeof(buffer));
                    if (read == 0) {
                        break;
                    }

                    total += read;

                    assert(fwrite(buffer, 1, read, fp) == (size_t)read);
                }

                Log::info("Done writing %d bytes to %s", total, path.c_str());

                fclose(fp);
            }
        }
    }

    if (args.log) {
        auto file_id = (file_id_t)FILE_ID_INVALID;
        auto file_position = (uint32_t)0;

        for (auto block = (block_index_t)0; block < geometry.number_of_blocks; ++block) {
            auto p = (uint8_t *)ptr + (uint64_t)block * geometry.block_size();
            auto &block_head = *(BlockHead *)p;

            if (block_head.valid()) {
                switch (block_head.type) {
                case BlockType::Index: {
                    if (block == 0) {
                        BlockLayout<FileTableHead, FileTableTail> layout{ storage, empty_allocator, { block, 0 }, BlockType::Index };
                        FileTableEntry entry;

                        sdebug() << "Block: " << block << " " << block_head << endl;

                        while (layout.walk(entry)) {
                            sdebug() << "  " << entry << endl;
                        }
                    }
                    else {
                        BlockLayout<IndexBlockHead, IndexBlockTail> layout{ storage, empty_allocator, { block, 0 }, BlockType::Index };
                        IndexRecord entry;

                        sdebug() << "Block: " << block << " " << block_head << endl;

                        while (layout.walk(entry)) {
                            sdebug() << "  " << entry << endl;
                        }
                    }
                    break;
                }
                case BlockType::File: {
                    auto &file_head = *(FileBlockHead *)p;

                    if (file_head.file_id != file_id) {
                        file_id = file_head.file_id;
                        file_position = 0;
                    }

                    sdebug() << "Block: " << block << " " << file_head << endl;

                    auto addr = BlockAddress(block, SectorSize);

                    for (auto sector = (sector_index_t)1; sector < geometry.sectors_per_block(); ++sector) {
                        auto sector_bytes = 0;

                        if (addr.tail_sector(geometry)) {
                            auto &file_tail = *(FileBlockTail *)((p + SectorSize * (sector + 1)) - sizeof(FileBlockTail));
                            if (file_tail.block.linked_block == 0) {
                                break;
                            }

                            sdebug() << "  " << file_tail << " file-pos=" << file_position << " sector=" << sector << " " << addr << endl;

                            sector_bytes = file_tail.sector.bytes;
                        }
                        else {
                            auto &sector_tail = *(FileSectorTail *)((p + SectorSize * (sector + 1)) - sizeof(FileSectorTail));
                            if (sector_tail.bytes == 0) {
                                break;
                            }

                            sdebug() << "  " << sector_tail << " file-pos=" << file_position << " sector=" << sector << " " << addr << endl;

                            sector_bytes = sector_tail.bytes;
                        }

                        file_position += sector_bytes;
                        addr = addr.advance(SectorSize);
                    }

                    break;
                }
                default: {
                    break;
                }
                }
            }
        }
    }

    storage.close();

    assert(munmap(ptr, file_size) == 0);

    close(fd);

    return 0;
}

