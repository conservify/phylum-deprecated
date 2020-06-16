#include <sys/stat.h>
#include <fcntl.h>

#include "record_walker.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/text_format.h>

#include "phylum_input_stream.h"

using namespace google::protobuf;
using namespace google::protobuf::io;
using namespace google::protobuf::compiler;

using namespace alogging;

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

bool RecordWalker::walk(PhylumInputStream &stream, RecordVisitor &visitor, bool verbose) {
    auto g = stream.geometry();
    auto message = get_message();
    assert(message != nullptr);

    while (true) {
        auto read_details = read(&stream, message);
        auto stream_address = stream.address();
        auto sector_address = stream_address.sector(g);
        auto block_position = stream_address.position;
        auto position = stream_address.block * g.block_size() + stream_address.position;

        if (read_details.is_eos) {
            break;
        }
        else if (read_details.is_incomplete) {
            auto first = corruption_at_ == 0;
            if (verbose) {
                std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << block_position << "]: " <<
                    "(" << (first ? "corruption-NEW" : "corruption-OLD") << ") incomplete @ " << position <<
                    " size = " << read_details.size << " bytes" << std::endl;
            }
            if (first) {
                corruption_at_ = position;
            }
            stream.skip_sector();
        }
        else if (!read_details) {
            auto first = corruption_at_ == 0;
            if (verbose) {
                std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << block_position << "]: " <<
                    "(" << (first ? "corruption-NEW" : "corruption-OLD") << ") failed @ " << position <<
                    " size = " << read_details.size << " bytes" << std::endl;
            }
            if (first) {
                corruption_at_ = position;
            }
            stream.skip_sector();
        }
        else {
            auto valid = read_details.size > 3; // NOTE: HACK
            if (!valid) {
                auto first = corruption_at_ == 0;
                if (verbose) {
                    std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << block_position << "]: " <<
                        "(" << (first ? "corruption-NEW" : "corruption-OLD") << ") failed @ " << position <<
                        " size = " << read_details.size << " bytes" << std::endl;
                }
                if (first) {
                    corruption_at_ = position;
                }
                stream.skip_sector();
            }
            else {
                if (corruption_at_ > 0) {
                    if (verbose) {
                        std::cerr << sector_address << " [" << std::setfill(' ') << std::setw(4) << block_position << "]: " <<
                            "(corruption-END) skipped to " << position << ", losing " << ((int64_t)position - corruption_at_) <<
                            " bytes (began = " << corruption_at_ << ")" << std::endl;
                    }
                }
                message_at_ = position;
                corruption_at_ = 0;
                visitor.message(stream, message, read_details.size);
            }
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

void PrintDetailsVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
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

static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) { return !std::isspace(ch); }));
}

static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(), s.end());
}

void LogsVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
    const auto descriptor = message->GetDescriptor();
    const auto reflection = message->GetReflection();

    const auto logField = descriptor->FindFieldByName("log");
    assert(logField != nullptr);

    const auto& logMessage = reflection->GetMessage(*message, logField);
    const auto logDescriptor = logMessage.GetDescriptor();
    const auto logReflection = logMessage.GetReflection();

    const auto timeField = logDescriptor->FindFieldByName("time");
    const auto uptimeField = logDescriptor->FindFieldByName("uptime");
    const auto levelField = logDescriptor->FindFieldByName("level");
    const auto facilityField = logDescriptor->FindFieldByName("facility");
    const auto lineField = logDescriptor->FindFieldByName("message");
    assert(lineField != nullptr);

    const auto time = logReflection->GetInt64(logMessage, timeField);
    const auto uptime = logReflection->GetUInt32(logMessage, uptimeField);
    const auto level = logReflection->GetUInt32(logMessage, levelField);
    const auto facility = logReflection->GetString(logMessage, facilityField);
    auto line = logReflection->GetString(logMessage, lineField);

    time_t t = time;
    struct tm *tm = localtime(&t);
    char date[20];
    strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", tm);

    rtrim(line);

    std::cout << date << " " << std::left << std::setw(10) << uptime << " " << std::left << std::setw(20) << facility << " " << line << std::endl;
}

void DataVisitor::message(PhylumInputStream &stream, Message *message, size_t serialized_size) {
    const auto descriptor = message->GetDescriptor();
    const auto reflection = message->GetReflection();


    if (!header_) {
        dump_header();
        header_ = true;
    }

    const auto statusField = descriptor->FindFieldByName("status");
    assert(statusField != nullptr);
    const auto metaField = descriptor->FindFieldByName("metadata");
    assert(metaField != nullptr);
    const auto loggedReadingField = descriptor->FindFieldByName("loggedReading");
    assert(loggedReadingField != nullptr);

    if (reflection->HasField(*message, statusField)) {
        const auto& statusMessage = reflection->GetMessage(*message, statusField);
        const auto statusDescriptor = statusMessage.GetDescriptor();
        const auto statusReflection = statusMessage.GetReflection();

        const auto timeField = statusDescriptor->FindFieldByName("time");
        const auto uptimeField = statusDescriptor->FindFieldByName("uptime");
        const auto batteryField = statusDescriptor->FindFieldByName("battery");

        const auto time = statusReflection->GetInt64(statusMessage, timeField);
        const auto uptime = statusReflection->GetUInt32(statusMessage, uptimeField);
        const auto battery = statusReflection->GetFloat(statusMessage, batteryField);

        time_t t = time;
        struct tm *tm = localtime(&t);
        char date[20];
        strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", tm);

        std::cout << "status," << date << "," << uptime << ",," << battery << std::endl;
    }

    if (reflection->HasField(*message, metaField)) {
        const auto& metaMessage = reflection->GetMessage(*message, metaField);
        const auto metaDescriptor = metaMessage.GetDescriptor();
        const auto metaReflection = metaMessage.GetReflection();

        const auto timeField = metaDescriptor->FindFieldByName("time");
        const auto deviceIdField = metaDescriptor->FindFieldByName("deviceId");
        const auto sensorsField = metaDescriptor->FindFieldByName("sensors");

        assert(timeField != nullptr);
        assert(deviceIdField != nullptr);
        assert(sensorsField != nullptr);

        const auto time = metaReflection->GetInt64(metaMessage, timeField);

        std::string deviceId;
        TextFormat::PrintFieldValueToString(metaMessage, deviceIdField, -1, &deviceId);

        time_t t = time;
        struct tm *tm = localtime(&t);
        char date[20];
        strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", tm);

        if (sensor_values_.size() > 0) {
            dump_readings();
        }

        if (true) {
            const auto &sensors = metaReflection->GetRepeatedPtrField<Message>(metaMessage, sensorsField);

            sensor_names_.clear();

            for (auto &sensorMessage : sensors) {
                const auto sensorDescriptor = sensorMessage.GetDescriptor();
                const auto sensorReflection = sensorMessage.GetReflection();
                const auto nameField = sensorDescriptor->FindFieldByName("name");
                const auto name = sensorReflection->GetString(sensorMessage, nameField);
                sensor_names_.push_back(name);
            }
        }

        std::cout << "meta," << date << ",," << deviceId << ",,,sensors,";

        auto first = true;
        for (auto &s : sensor_names_) {
            if (!first) {
                std::cout << ",";
            }
            std::cout << s;
            first = false;
        }

        std::cout << std::endl;
    }

    if (reflection->HasField(*message, loggedReadingField)) {
        const auto& loggedReadingMessage = reflection->GetMessage(*message, loggedReadingField);
        const auto loggedReadingDescriptor = loggedReadingMessage.GetDescriptor();
        const auto loggedReadingReflection = loggedReadingMessage.GetReflection();

        const auto readingMessageField = loggedReadingDescriptor->FindFieldByName("reading");
        const auto& readingMessage = loggedReadingReflection->GetMessage(loggedReadingMessage, readingMessageField);
        const auto readingDescriptor = readingMessage.GetDescriptor();
        const auto readingReflection = readingMessage.GetReflection();

        const auto timeField = readingDescriptor->FindFieldByName("time");
        const auto readingField = readingDescriptor->FindFieldByName("reading");
        const auto valueField = readingDescriptor->FindFieldByName("value");

        auto time = readingReflection->GetInt64(readingMessage, timeField);
        auto reading = readingReflection->GetUInt64(readingMessage, readingField);
        auto value = readingReflection->GetFloat(readingMessage, valueField);

        if (reading > 0) {
            if (reading_ != reading) {
                dump_readings();
                reading_ = reading;
                time_ = time;
            }

            sensor_values_.push_back(value);
        }
    }
}

void DataVisitor::dump_header() {
    std::string names[] = {
        "record type",
        "time",
        "uptime (ms)",
        "device id",
        "battery",
        "record",
        "notes", // sensors/mismatch
        "sensors",
    };

    auto first = true;
    for (auto &n : names) {
        if (!first) {
            std::cout << ",";
        }
        std::cout << n;
        first = false;
    }

    std::cout << std::endl;
}

void DataVisitor::dump_readings() {
    if (reading_ == 0) {
        return;
    }

    time_t t = time_;
    struct tm *tm = localtime(&t);
    char date[20];
    strftime(date, sizeof(date), "%Y/%m/%d %H:%M:%S", tm);

    std::cout << "readings,";
    std::cout << date;
    std::cout << ",,,,";
    std::cout << reading_;
    std::cout << ",";

    if (sensor_values_.size() > 0 && sensor_values_.size() != sensor_names_.size()) {
        std::cout << "mismatch " << sensor_values_.size() << " " << sensor_names_.size() << "";
    }

    if (sensor_values_.size() > 0) {
        auto first = true;
        for (auto v : sensor_values_) {
            if (first) {
                first = false;
                // continue;
            }
            std::cout << "," << v;
        }
    }

    std::cout << std::endl;

    sensor_values_.clear();
}
