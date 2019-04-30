#pragma once

#include <string>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "phylum_input_stream.h"

class RecordVisitor {
public:
    virtual void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) = 0;

};

class LoggingVisitor : public RecordVisitor {
public:
    void message(PhylumInputStream &stream, google::protobuf::Message *message, size_t serialized_size) override;

};

class RecordWalker {
private:
    std::string proto_file_;
    std::string message_name_;

public:
    struct Read {
        bool is_eos;
        bool is_failed;
        bool is_incomplete;
        size_t size;

        operator bool() const {
            return !is_failed && !is_incomplete && !is_eos;
        }

        static Read eos() { return { true, false, false, 0 }; };
        static Read failed(size_t size) { return { false, true, false, size }; };
        static Read incomplete(size_t size) { return { false, false, true, size }; };
        static Read success(size_t size) { return { false, false, false, size }; };
    };

public:
    RecordWalker(std::string proto_file, std::string message_name);

public:
    bool walk(PhylumInputStream &stream, RecordVisitor &visitor);
    Read read(google::protobuf::io::ZeroCopyInputStream *ri, google::protobuf::MessageLite *message);

};
