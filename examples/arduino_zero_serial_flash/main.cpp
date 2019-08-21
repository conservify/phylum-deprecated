#include <Arduino.h>

#include <phylum/phylum.h>
#include <phylum/basic_super_block_manager.h>
#include <phylum/files.h>
#include <backends/arduino_serial_flash/arduino_serial_flash.h>
#include <backends/arduino_serial_flash/serial_flash_allocator.h>

using namespace alogging;
using namespace phylum;

typedef struct board_configuration_t {
    uint8_t periph_enable;
    uint8_t flash_cs;
    uint8_t sd_cs;
    uint8_t wifi_cs;
    uint8_t rfm95_cs;
} board_configuration_t;

board_configuration_t possible_boards[4] = {
    { 25u, 26u, 12u,  7u,  5u }, // Core
    {  8u,  6u,  0u,  0u,  5u }, // Sonar
    { 12u,  5u,  0u,  0u,  0u }, // Atlas
    {  8u,  5u,  0u,  0u,  0u }, // Weather
};

struct SimpleState : MinimumSuperBlock {
};

using OurStateManager = BasicSuperBlockManager<SimpleState>;

static void fail() {
    Serial.println("Fail!");
    while (true) {
        delay(100);
    }
}

extern "C" char *sbrk(int32_t i);

static uint32_t free_memory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}

class FileWriter {
private:
    File *file_{ nullptr };

public:
    FileWriter() {
    }

    FileWriter(phylum::BlockedFile &file) : file_(&file) {
    }

public:
    int32_t write(uint8_t *ptr, size_t size) {
        return file_->write(ptr, size, true);
    }

    void close() {
        file_->close();
    }

};

class FileStorage {
private:
    Files *files_;
    AllocatedBlockedFile opened_;
    FileWriter writer_;

public:
    FileStorage(Files &files) : files_(&files) {
    }

public:
    BlockAddress beginningOfOpenFile() {
        return opened_.beginning();
    }

    FileWriter *write() {
        opened_ = files_->open({ }, phylum::OpenMode::Write);

        if (!opened_.format()) {
            // NOTE: Just return a /dev/null writer?
            assert(false);
            return nullptr;
        }

        writer_ = FileWriter{ opened_ };

        return &writer_;
    }

};

static void super_block_tests(ArduinoSerialFlashBackend &storage, SerialFlashAllocator &allocator) {
    OurStateManager sbm{ storage, allocator };

    sdebug() << "Initialize FS" << endl;

    if (!sbm.locate()) {
        sdebug() << "Locate failed, creating..." << endl;

        if (!sbm.create()) {
            sdebug() << "Create failed" << endl;
            fail();
        }

        if (!sbm.locate()) {
            sdebug() << "Locate failed" << endl;
            fail();
        }
    }

    sdebug() << "Ready!" << endl;

    for (auto i = 0; i < 256; ++i) {
        if (!sbm.save()) {
            sdebug() << "Save failed!" << endl;
            fail();
        }

        sdebug() << "New Location: " << sbm.location() << endl;
    }
}

static void file_tests_1(ArduinoSerialFlashBackend &storage, SerialFlashAllocator &allocator) {
    Files files{ &storage, &allocator };

    auto file1 = files.open({ }, OpenMode::Write);

    if (!file1.initialize()) {
        fail();
    }

    if (!file1.format()) {
        fail();
    }

    auto location = file1.beginning();

    auto total = 0;
    uint8_t buffer[163] = { 0xee };

    for (auto i = 0; i < 388; ++i) {
        auto wrote = file1.write(buffer, sizeof(buffer), true);
        if (wrote <= 0) {
            fail();
        }
        total += wrote;
    }

    auto wrote = file1.write(buffer, 16, true);
    if (wrote <= 0) {
        fail();
    }
    total += wrote;

    file1.close();

    sdebug() << "Wrote: " << (uint32_t)total << endl;

    auto file2 = files.open(location, OpenMode::Read);

    file2.seek(UINT64_MAX);

    sdebug() << "Size: " << (uint32_t)file2.size() << endl;
}

static void file_tests_2(ArduinoSerialFlashBackend &storage, SerialFlashAllocator &allocator) {
    Files files{ &storage, &allocator };
    FileStorage fileStorage{ files };

    auto writer = fileStorage.write();
    auto location = fileStorage.beginningOfOpenFile();

    auto total = 0;
    uint8_t buffer[163] = { 0xee };

    for (auto i = 0; i < 388; ++i) {
        auto wrote = writer->write(buffer, sizeof(buffer));
        if (wrote <= 0) {
            fail();
        }
        total += wrote;
    }

    auto wrote = writer->write(buffer, 16);
    if (wrote <= 0) {
        fail();
    }
    total += wrote;

    writer->close();

    sdebug() << "Wrote: " << (uint32_t)total << endl;

    auto file2 = files.open(location, OpenMode::Read);
    file2.seek(UINT64_MAX);
    sdebug() << "Size: " << (uint32_t)file2.size() << endl;
}

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    sdebug() << "Initialize Storage" << endl;

    NoopStorageBackendCallbacks callbacks;
    ArduinoSerialFlashBackend storage{ callbacks };
    auto success = false;
    for (auto cfg : possible_boards) {
        if (cfg.periph_enable > 0) {
            pinMode(cfg.periph_enable, OUTPUT);
            digitalWrite(cfg.periph_enable, LOW);
            delay(100);
            digitalWrite(cfg.periph_enable, HIGH);
            delay(100);
        }

        if (cfg.rfm95_cs > 0) {
            pinMode(cfg.rfm95_cs, OUTPUT);
            digitalWrite(cfg.rfm95_cs, HIGH);
        }

        if (cfg.sd_cs > 0) {
            pinMode(cfg.sd_cs, OUTPUT);
            digitalWrite(cfg.sd_cs, HIGH);
        }

        if (cfg.wifi_cs > 0) {
            pinMode(cfg.wifi_cs, OUTPUT);
            digitalWrite(cfg.wifi_cs, HIGH);
        }

        if (cfg.flash_cs > 0) {
            pinMode(cfg.flash_cs, OUTPUT);
            digitalWrite(cfg.flash_cs, HIGH);
        }

        if (storage.initialize(cfg.flash_cs, 2048)) {
            sdebug() << "Found on #" << cfg.flash_cs << endl;
            success = true;
            break;
        }
    }

    if (!success) {
        fail();
    }

    if (!storage.open()) {
        fail();
    }

    sdebug() << "Initialize Allocator" << endl;

    SerialFlashAllocator allocator{ storage };
    if (!allocator.initialize()) {
        fail();
    }

    super_block_tests(storage, allocator);

    file_tests_1(storage, allocator);

    file_tests_2(storage, allocator);

    sdebug() << "DONE!" << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
