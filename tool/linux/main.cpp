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

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>

#include "phylum_input_stream.h"

namespace fs = std::experimental::filesystem;

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;

using namespace phylum;

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

    Log::info("Opening %s...", file_name);

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

            if (!args.log) {
                if (!walk_protobuf_records(geometry, (uint8_t *)ptr, opened.head().block)) {
                    return false;
                }
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

using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

bool pb_read_delimited_from(google::protobuf::io::ZeroCopyInputStream *ri, google::protobuf::MessageLite *message) {
    CodedInputStream cis(ri);

    uint32_t size;
    if (!cis.ReadVarint32(&size)) {
        return false;
    }

    auto limited = cis.PushLimit(size);

    if (!message->MergeFromCodedStream(&cis)) {
        sdebug() << "Failed to merge" << endl;
        return false;
    }

    if (!cis.ConsumedEntireMessage()) {
        sdebug() << "Incomplete message" << endl;
        return false;
    }

    cis.PopLimit(limited);

    return true;
}

bool walk_protobuf_records(Geometry geometry, uint8_t *everything, uint32_t block) {
    std::string message_type("DataRecord");

    auto fd = open("/home/jlewallen/fieldkit/data-protocol/fk-data.proto", O_RDONLY);
    if (fd < 0) {
        return false;
    }

    FileInputStream fis(fd);
    fis.SetCloseOnDelete(true);

    Tokenizer tokenizer(&fis, nullptr);

    FileDescriptorProto file_desc_proto;
    Parser parser;
    if (!parser.Parse(&tokenizer, &file_desc_proto)) {
        return false;
    }

    if (!file_desc_proto.has_name()) {
        file_desc_proto.set_name(message_type);
    }

    DescriptorPool pool;
    auto file_desc = pool.BuildFile(file_desc_proto);
    if (file_desc == nullptr) {
        return false;
    }

    auto message_desc = file_desc->FindMessageTypeByName(message_type);
    if (message_desc == nullptr) {
        return false;
    }

    DynamicMessageFactory factory;
    auto prototype_msg = factory.GetPrototype(message_desc);
    assert(prototype_msg != nullptr);

    auto mutable_msg = prototype_msg->New();
    assert(mutable_msg != nullptr);

    PhylumInputStream is{ geometry, everything, block };
    while (true) {
        if (!pb_read_delimited_from(&is, mutable_msg)) {
            if (is.position() != 0) {
                sdebug() << "Done: " << is.position()  << endl;
            }
            break;
        }

        std::vector<const FieldDescriptor*> fields;
        auto reflection = mutable_msg->GetReflection();
        reflection->ListFields(*mutable_msg, &fields);

        for (auto field_iter = fields.begin(); field_iter != fields.end(); field_iter++) {
            auto field = *field_iter;
            assert(field != nullptr);

            switch (field->type()) {
            case FieldDescriptor::TYPE_BOOL: {
                break;
            }
            case FieldDescriptor::TYPE_INT32: {
                break;
            }
            case FieldDescriptor::TYPE_INT64: {
                break;
            }
            case FieldDescriptor::TYPE_FLOAT: {
                break;
            }
            case FieldDescriptor::TYPE_STRING: {
                break;
            }
            case FieldDescriptor::TYPE_MESSAGE: {
                break;
            }
            }
        }
    }

    return true;
}
