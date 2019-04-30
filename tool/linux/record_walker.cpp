#include <sys/stat.h>
#include <fcntl.h>

#include "record_walker.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>

#include "phylum_input_stream.h"

using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

RecordWalker::RecordWalker(std::string proto_file, std::string message_name)
    : proto_file_(proto_file), message_name_(message_name) {
}

google::protobuf::Message *RecordWalker::get_message() {
    auto message_desc = pool.FindMessageTypeByName(full_message_name_);
    if (message_desc == nullptr) {
        auto fd = open(proto_file_.c_str(), O_RDONLY);
        assert(fd != 0);

        FileInputStream fis(fd);
        fis.SetCloseOnDelete(true);

        FileDescriptorProto file_desc_proto;

        Parser parser;
        Tokenizer tokenizer(&fis, nullptr);
        assert(parser.Parse(&tokenizer, &file_desc_proto));

        if (!file_desc_proto.has_name()) {
            file_desc_proto.set_name(message_name_);
        }

        auto file_desc = pool.BuildFile(file_desc_proto);
        assert(file_desc != nullptr);

        message_desc = file_desc->FindMessageTypeByName(message_name_);
        assert(message_desc != nullptr);

        full_message_name_ = message_desc->full_name();
    }

    auto prototype_msg = factory.GetPrototype(message_desc);
    assert(prototype_msg != nullptr);

    auto message = prototype_msg->New();
    assert(message != nullptr);

    return message;
}

bool RecordWalker::walk(PhylumInputStream &stream, RecordVisitor &visitor) {
    auto message = get_message();
    assert(message != nullptr);

    while (true) {
        auto read_details = read(&stream, message);
        auto stream_address = stream.address();
        auto sector_address = stream_address.sector(stream.geometry());
        auto position = stream_address.position;

        if (read_details.is_eos) {
            break;
        }
        else if (read_details.is_incomplete) {
            std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << position <<
                "]: incomplete message @ " << stream.position() << " size = " << read_details.size << " bytes" << std::endl;
        }
        else if (!read_details) {
            std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << position <<
                "]: failed message @ " << stream.position() << " size = " << read_details.size << " bytes" << std::endl;
        }
        else {
            visitor.message(stream, message, read_details.size);
        }

        message->Clear();
    }

    return true;
}

uint32_t RecordWalker::single(uint8_t *ptr) {
    auto message = get_message();
    assert(message != nullptr);

    ArrayInputStream ais(ptr, 510);

    CodedInputStream cis(&ais);

    uint32_t size;

    if (!cis.ReadVarint32(&size)) {
        return 0;
    }

    std::cerr << size << endl;

    auto limited = cis.PushLimit(size);

    if (!message->MergeFromCodedStream(&cis)) {
        return 0;
    }

    if (!cis.ConsumedEntireMessage()) {
        return 0;
    }

    cis.PopLimit(limited);

    auto bytes_read = cis.CurrentPosition();
    auto debug = message->ShortDebugString();

    std::cerr << "(" << std::setfill(' ') << std::setw(3) << size << ") " << debug << std::endl;

    return bytes_read;
}

RecordWalker::Read RecordWalker::read(ZeroCopyInputStream *ri, MessageLite *message) {
    CodedInputStream cis(ri);

    uint32_t size;

    if (!cis.ReadVarint32(&size)) {
        return Read::eos();
    }

    auto limited = cis.PushLimit(size);

    if (!message->MergeFromCodedStream(&cis)) {
        return Read::failed(size);
    }

    if (!cis.ConsumedEntireMessage()) {
        return Read::incomplete(size);
    }

    auto bytes_read = cis.CurrentPosition();

    cis.PopLimit(limited);

    return Read::success(bytes_read);
}

void NoopVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
}

void LoggingVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
    auto stream_address = stream.address();
    auto sector_address = stream_address.sector(stream.geometry());
    auto position = stream_address.position;

    auto debug = message->ShortDebugString();

    std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << position <<
        "] (" << std::setfill(' ') << std::setw(3) << serialized_size << ") " << debug << std::endl;
}

void StreamVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
    auto sector_address = stream.address().sector(stream.geometry());
    auto debug = message->ShortDebugString();
    stream_ << "" << sector_address << " " << debug << std::endl;
}
