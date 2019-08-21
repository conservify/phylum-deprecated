#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>

using namespace alogging;
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
static void write_file(FileSystem &fs, const char *name) {
    auto started = millis();
    auto file = fs.open(name);
    auto wrote = 0;
    while (wrote < FileSize) {
        if (file.write("Jacob", 5) != 5) {
            fail();
        }
        wrote += 5;
    }

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

    sdebug() << "Bytes: " << file.size() << endl;
    sdebug() << "Duration: " << elapsed << endl;
    sdebug() << "Bytes/s: " << file.size() / float(elapsed) << endl;

    file.close();
}

template<size_t BufferSize>
static void read_file(FileSystem &fs, const char *name) {
    auto started = millis();
    auto file = fs.open(name, true);
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

    sdebug() << "Bytes: " << file.size() << endl;
    sdebug() << "Duration: " << elapsed << endl;
    sdebug() << "Bytes/s: " << read / float(elapsed) << endl;

    file.close();
}

static void seek_end_file(FileSystem &fs, const char *name) {
    auto started = millis();
    auto file = fs.open(name, true);

    file.seek(UINT32_MAX);

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

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

    sdebug() << "Starting: " << free_memory() << endl;

    sdebug() << "sizeof(FileSystem): " << sizeof(FileSystem) << endl;
    sdebug() << "sizeof(OpenFile): " << sizeof(OpenFile) << endl;
    sdebug() << "sizeof(SequentialBlockAllocator): " << sizeof(SequentialBlockAllocator) << endl;
    sdebug() << "sizeof(ArduinoSdBackend): " << sizeof(ArduinoSdBackend) << endl;
    sdebug() << "sizeof(Node): " << sizeof(Node<uint64_t, uint64_t, BlockAddress, 6, 6>) << endl;

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

    SequentialBlockAllocator allocator;
    FileSystem fs{ storage, allocator };
    if (!fs.mount(true)) {
        fail();
    }

    sdebug() << endl << "Creating small file..." << endl;
    write_file<128>(fs, "small.bin");

    sdebug() << endl << "Creating file..." << endl;
    write_file<1024 * 1024>(fs, "large.bin");

    sdebug() << endl << "Reading file (256)..." << endl;
    read_file<256>(fs, "large.bin");

    sdebug() << endl << "Reading file (512)..." << endl;
    read_file<512>(fs, "large.bin");

    sdebug() << endl << "Seek end of file..." << endl;
    seek_end_file(fs, "large.bin");

    sdebug() << endl << "Done: " << free_memory() << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
