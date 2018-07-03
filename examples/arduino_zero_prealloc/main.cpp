#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>

using namespace phylum;

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

template<size_t FileSize>
static void write_file(FileOpener &fs, FileDescriptor &fd) {
    auto started = millis();
    auto file = fs.open(fd, OpenMode::Write);
    auto wrote = 0;
    while (wrote < FileSize) {
        if (file.write((uint8_t *)"Jacob", 5) != 5) {
            fail();
        }
        wrote += 5;
    }

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

    sdebug() << "Position: " << file.tell() << endl;
    sdebug() << "Bytes: " << file.size() << endl;
    sdebug() << "Duration: " << elapsed << endl;
    sdebug() << "Bytes/s: " << file.size() / float(elapsed) << endl;

    file.close();

    sdebug() << "Done" << endl;
}

template<size_t BufferSize>
static void read_file(FileOpener &fs, FileDescriptor &fd) {
    auto started = millis();
    auto file = fs.open(fd, OpenMode::Read);
    auto read = 0;
    while (true) {
        uint8_t buffer[BufferSize];
        auto bytes = file.read(buffer, sizeof(buffer));
        if (bytes == 0) {
            break;
        }

        read += bytes;
    }

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

    sdebug() << "Position: " << file.tell() << endl;
    sdebug() << "Bytes: " << file.size() << endl;
    sdebug() << "Duration: " << elapsed << endl;
    sdebug() << "Bytes/s: " << read / float(elapsed) << endl;

    file.close();

    sdebug() << "Done" << endl;
}

static void seek_end_file(FileOpener &fs, FileDescriptor &fd) {
    auto started = millis();
    auto file = fs.open(fd, OpenMode::Read);

    file.seek(UINT64_MAX);

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

    sdebug() << "Position: " << file.tell() << endl;
    sdebug() << "Bytes: " << file.size() << endl;
    sdebug() << "Duration: " << elapsed << endl;
    sdebug() << "Bytes/s: " << file.size() / float(elapsed) << endl;

    file.close();
}

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    sdebug() << 3.14159 << endl;

    sdebug() << "Starting: " << free_memory() << endl;

    sdebug() << "sizeof(uint32_t): " << sizeof(uint32_t) << endl;
    sdebug() << "UINT32_MAX: " << UINT32_MAX << endl;

    sdebug() << "sizeof(uint64_t): " << sizeof(uint64_t) << endl;
    sdebug() << "UINT64_MAX: " << UINT64_MAX << endl;

    sdebug() << "sizeof(uint_least64_t): " << sizeof(uint_least64_t) << endl;
    sdebug() << "UINT_LEAST64_MAX: " << UINT_LEAST64_MAX << endl;

    sdebug() << "Initialize Backend" << endl;

    Geometry g{ 0, 4, 4, SectorSize };
    ArduinoSdBackend storage;
    if (!storage.initialize(g, 12)) {
        fail();
    }

    if (!storage.open()) {
        fail();
    }

    sdebug() << "Initialize FS" << endl;

    FileDescriptor file_system_area_fd =   { "system",        100 };
    FileDescriptor file_log_startup_fd =   { "startup.log",   100 };
    FileDescriptor file_log_now_fd =       { "now.log",       100 };
    FileDescriptor file_log_emergency_fd = { "emergency.log", 100 };
    FileDescriptor file_data_fk =          { "data.fk",       0   };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_log_startup_fd,
        &file_log_now_fd,
        &file_log_emergency_fd,
        &file_data_fk
    };

    FileLayout<5> fs{ storage };

    if (!fs.format(files)) {
        fail();
    }

    sdebug() << endl << "Creating small file..." << endl;
    write_file<128>(fs, file_log_startup_fd);

    sdebug() << endl << "Creating file..." << endl;
    write_file<1024 * 1024>(fs, file_log_now_fd);

    sdebug() << endl << "Reading file (256)..." << endl;
    read_file<256>(fs, file_log_now_fd);

    sdebug() << endl << "Reading file (512)..." << endl;
    read_file<512>(fs, file_log_now_fd);

    sdebug() << endl << "Seek end of file..." << endl;
    seek_end_file(fs, file_log_now_fd);

    sdebug() << endl << "Done: " << free_memory() << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
