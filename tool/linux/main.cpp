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

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>

#include <google/protobuf/compiler/parser.h>

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

using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

class PhylumInputStream : public ZeroCopyInputStream {
public:
    PhylumInputStream(Geometry geometry, uint8_t *everything, block_index_t block);

    // implements ZeroCopyInputStream ----------------------------------
    bool Next(const void** data, int* size);
    void BackUp(int count);
    bool Skip(int count);
    int64 ByteCount() const;

public:
    uint32_t position() const {
        return position_;
    }

private:
    Geometry geometry_;
    uint8_t *everything_;
    uint8_t *iter_;
    BlockAddress address_;
    uint32_t position_;
    uint32_t sector_remaining_;

    struct Block {
        uint8_t *ptr;
        size_t size;
    };

    Block previous_block_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(PhylumInputStream);
};

PhylumInputStream::PhylumInputStream(Geometry geometry, uint8_t *everything, block_index_t block)
    : geometry_(geometry), everything_(everything), address_{ block, 0 }, iter_{ nullptr }, position_{ 0 }, sector_remaining_{ 0 } {
}

bool PhylumInputStream::Next(const void **data, int *size) {
    auto &g = geometry_;

    *data == nullptr;
    *size = 0;

    while (true) {
        if (sector_remaining_ == 0) {
            auto follow = address_.tail_sector(g);
            auto block_ptr = everything_ + address_.block * g.block_size();

            address_.position += address_.remaining_in_sector(g);

            auto sector = address_.sector_number(g);

            if (address_.tail_sector(g)) {
                auto &sector_tail = *(FileBlockTail *)((block_ptr + (SectorSize * g.sectors_per_block())) - sizeof(FileBlockTail));

                if (follow) {
                    address_ = BlockAddress{ sector_tail.block.linked_block, 0 };
                    sector_remaining_ = 0;
                    continue;
                }

                sector_remaining_ = sector_tail.sector.bytes;
            }
            else {
                auto &sector_tail = *(FileSectorTail *)((block_ptr + (SectorSize * (sector + 1))) - sizeof(FileSectorTail));
                sector_remaining_ = sector_tail.bytes;
            }

            iter_ = everything_ + (address_.block * g.block_size()) + address_.position;

            if (sector_remaining_ == 0) {
                return false;
            }
        }

        break;
    }

    previous_block_ = Block{ iter_, sector_remaining_ };

    *data = previous_block_.ptr;
    *size = previous_block_.size;

    position_ += sector_remaining_;
    sector_remaining_ = 0;

    return true;
}

void PhylumInputStream::BackUp(int c) {
    sector_remaining_ = c;
    iter_ = previous_block_.ptr + (previous_block_.size - c);
    position_ -= c;
}

bool PhylumInputStream::Skip(int c) {
    assert(false);
    return true;
}

int64 PhylumInputStream::ByteCount() const {
    assert(false);
    return 0;
}

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

bool walk_protobuf_records2(Geometry geometry, uint8_t *everything, uint32_t block) {
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

bool walk_protobuf_records(Geometry geometry, uint8_t *everything, uint32_t block) {
    assert(walk_protobuf_records2(geometry, everything, block));

    return true;

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

    return false;
}
