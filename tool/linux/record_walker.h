#pragma once

#include <string>

#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include "phylum_input_stream.h"

class RecordVisitor {
public:
};

class LoggingVisitor : public RecordVisitor {
public:
};

class RecordWalker {
private:
    std::string proto_file_;
    std::string message_name_;

public:
    RecordWalker(std::string proto_file, std::string message_name);

public:
    bool walk(PhylumInputStream &stream, RecordVisitor &visitor);
    bool read(google::protobuf::io::ZeroCopyInputStream *ri, google::protobuf::MessageLite *message);

};
