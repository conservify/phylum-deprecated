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

bool RecordWalker::walk(PhylumInputStream &stream, RecordVisitor &visitor) {
    auto fd = open(proto_file_.c_str(), O_RDONLY);
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
        file_desc_proto.set_name(message_name_);
    }

    DescriptorPool pool;
    auto file_desc = pool.BuildFile(file_desc_proto);
    if (file_desc == nullptr) {
        return false;
    }

    auto message_desc = file_desc->FindMessageTypeByName(message_name_);
    if (message_desc == nullptr) {
        return false;
    }

    DynamicMessageFactory factory;
    auto prototype_msg = factory.GetPrototype(message_desc);
    assert(prototype_msg != nullptr);

    auto mutable_msg = prototype_msg->New();
    assert(mutable_msg != nullptr);

    while (true) {
        auto read_details = read(&stream, mutable_msg);
        if (read_details.is_eos) {
            break;
        }
        else if (read_details.is_incomplete) {
            sdebug() << "Incomplete message @ " << stream.position() << " size = " << read_details.size << " bytes" << endl;
        }
        else if (!read_details) {
            sdebug() << "Failed message @ " << stream.position() << " size = " << read_details.size << " bytes" << endl;
        }
        else {
            visitor.message(mutable_msg, read_details.size);
        }
    }

    return true;
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

    cis.PopLimit(limited);

    return Read::success(size);
}

void LoggingVisitor::message(Message *message, size_t serialized_size) {
    // std::cout << "Message<" << ">" << std::endl;
}
