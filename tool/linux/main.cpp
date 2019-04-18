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

#include <fk-data-protocol.h>

namespace fs = std::experimental::filesystem;

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;
using namespace phylum;

struct pb_phylum_reader_state_t {
    pb_byte_t *everything;
    pb_byte_t *iter;
    uint32_t position;
    uint32_t sector_remaining;
    BlockAddress addr;
    Geometry geometry;
};

bool pb_buf_read(pb_istream_t *stream, pb_byte_t *buf, size_t c);
bool pb_decode_string(pb_istream_t *stream, const pb_field_t *, void **arg);
bool walk_protobuf_records(Geometry geometry, uint8_t *everything, uint32_t block);

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
    void block(block_index_t block) override {
        Log::info("Block: %d", block);
    }

    bool is_free(block_index_t block) {
        return false;
    }

};

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

    Log::info("Opening %s (%ld bytes)...", file_name, file_size);

    auto fd = open(file_name, O_RDONLY, 0);
    assert(fd != -1);

    Log::info("Mapping...");

    auto ptr = mmap(nullptr, file_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    assert(ptr != MAP_FAILED);

    Log::info("Mounting...");

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

    if (false) {
        auto data_alloc = fs.allocation(4);
        auto head = data_alloc.data.beginning();
        SerialFlashAllocator allocator{ storage };
        AllocatedBlockedFile file{ &storage, OpenMode::Read, &allocator, head };
        BlockLogger block_logger;
        file.walk(&block_logger);
    }

    for (auto fd : descriptors) {
        auto opened = fs.open(*fd);
        if (opened) {
            Log::info("File: %s size = %d", fd->name, (uint32_t)opened.size());

            if (!opened.seek(0)) {
                Log::error("Error seeking to beginning of file!");
                return 2;
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

                        if (!walk_protobuf_records(geometry, (uint8_t *)ptr, block)) {
                            return false;
                        }
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

bool pb_buf_read(pb_istream_t *stream, pb_byte_t *buf, size_t c) {
    auto &state = *(pb_phylum_reader_state_t *)stream->state;
    auto &g = state.geometry;

    for (auto i = (size_t)0; i < c; ) {
        if (state.sector_remaining == 0) {
            auto follow = state.addr.tail_sector(g);
            auto block_ptr = state.everything + state.addr.block * g.block_size();

            state.addr.position += state.addr.remaining_in_sector(g);

            auto sector = state.addr.sector_number(g);

            if (state.addr.tail_sector(g)) {
                auto &sector_tail = *(FileBlockTail *)((block_ptr + (SectorSize * g.sectors_per_block())) - sizeof(FileBlockTail));

                if (follow) {
                    state.addr = BlockAddress{ sector_tail.block.linked_block, 0 };
                    state.sector_remaining = 0;
                    continue;
                }

                state.sector_remaining = sector_tail.sector.bytes;
            }
            else {
                auto &sector_tail = *(FileSectorTail *)((block_ptr + (SectorSize * (sector + 1))) - sizeof(FileSectorTail));
                state.sector_remaining = sector_tail.bytes;
            }

            state.iter = state.everything + (state.addr.block * g.block_size()) + state.addr.position;

            if (state.sector_remaining == 0) {
                return false;
            }
        }

        if (buf != nullptr) {
            buf[i] = *state.iter;
        }

        state.addr.position++;
        state.sector_remaining--;
        state.position++;
        state.iter++;
        i++;
    }

    return true;
}

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *, void **arg) {
    auto len = stream->bytes_left;

    if (len == 0) {
        (*arg) = (void *)"";
        return true;
    }

    auto *ptr = (uint8_t *)malloc(len + 1);
    if (!pb_read(stream, ptr, len)) {
        return false;
    }

    ptr[len] = 0;

    (*arg) = (void *)ptr;

    return true;
}

bool pb_search_for_record(pb_istream_t *stream) {
    return true;
}

bool walk_protobuf_records(Geometry geometry, uint8_t *everything, uint32_t block) {
    auto block_ptr = everything + (geometry.block_size() * block);
    auto &first_sector_tail = *(FileSectorTail *)((block_ptr + SectorSize * 2) - sizeof(FileSectorTail));

    if (first_sector_tail.bytes == 0) {
        return true;
    }

    fk_data_DataRecord message = fk_data_DataRecord_init_default;
    message.log.facility.funcs.decode = pb_decode_string;
    message.log.message.funcs.decode = pb_decode_string;

    pb_phylum_reader_state_t state;
    state.everything = everything;
    state.geometry = geometry;
    state.addr = BlockAddress{ block, 0 };
    state.iter = nullptr;
    state.sector_remaining = 0;
    state.position = 0;

    pb_istream_t stream;
    stream.state = &state;
    stream.bytes_left = UINT32_MAX; // TODO: Make this accurate.
    stream.callback = &pb_buf_read;
    stream.errmsg = nullptr;

    while (true) {
        pb_istream_t message_stream;
        if (!pb_make_string_substream(&stream, &message_stream)) {
            sdebug() << "Error creating message substream." << endl;
            return false;
        }

        auto message_size = message_stream.bytes_left;

        if (!pb_decode(&message_stream, fk_data_DataRecord_fields, &message)) {
            if (PB_GET_ERROR(&message_stream) != nullptr) {
                sdebug() << "Error decoding: " << PB_GET_ERROR(&message_stream) << " message-size=" << message_size << endl;
            }
            else {
                sdebug() << "Error decoding!" << " message-size=" << message_size << endl;
            }
        }

        if (!pb_close_string_substream(&stream, &message_stream)) {
            sdebug() << "Error closing message substream. bytes-left=" << message_stream.bytes_left << endl;
            return false;
        }

        if (message.log.message.arg != nullptr) {
            auto &lm = message.log;
            auto facility = (const char *)lm.facility.arg;
            auto message = (const char *)lm.message.arg;
            sdebug() << state.position << " " << facility << ": '" << message << "'" << endl;
        }
    }

    // __builtin_trap();
    return false;
}
