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

    sdebug() << "Bytes: " << file.size() << std::endl;
    sdebug() << "Duration: " << elapsed << std::endl;
    sdebug() << "Bytes/s: " << file.size() / float(elapsed) << std::endl;

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

    sdebug() << "Bytes: " << file.size() << std::endl;
    sdebug() << "Duration: " << elapsed << std::endl;
    sdebug() << "Bytes/s: " << read / float(elapsed) << std::endl;

    file.close();
}

static void seek_end_file(FileSystem &fs, const char *name) {
    auto started = millis();
    auto file = fs.open(name, true);

    file.seek(UINT32_MAX);

    auto finished = millis();
    auto elapsed = float(finished - started) / 1000.0f;

    sdebug() << "Bytes: " << file.size() << std::endl;
    sdebug() << "Duration: " << elapsed << std::endl;
    sdebug() << "Bytes/s: " << file.size() / float(elapsed) << std::endl;

    file.close();
}

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    sdebug() << "Starting: " << free_memory() << std::endl;

    sdebug() << "sizeof(FileSystem): " << sizeof(FileSystem) << std::endl;
    sdebug() << "sizeof(OpenFile): " << sizeof(OpenFile) << std::endl;
    sdebug() << "sizeof(SequentialBlockAllocator): " << sizeof(SequentialBlockAllocator) << std::endl;
    sdebug() << "sizeof(ArduinoSdBackend): " << sizeof(ArduinoSdBackend) << std::endl;
    sdebug() << "sizeof(Node): " << sizeof(Node<uint64_t, uint64_t, BlockAddress, 6, 6>) << std::endl;

    sdebug() << "Initialize Backend" << std::endl;

    Geometry g{ 0, 4, 4, SectorSize };
    ArduinoSdBackend storage;
    if (!storage.initialize(g, 12)) {
        fail();
    }

    sdebug() << "Initialize FS" << std::endl;

    SequentialBlockAllocator allocator{ storage.geometry() };
    FileSystem fs{ storage, allocator };
    if (!fs.initialize(true)) {
        fail();
    }

    sdebug() << "Creating small file..." << std::endl;
    {
        auto file = fs.open("small.bin");
        if (!file.write("Jacob", 5)) {
            fail();
        }

        sdebug() << "Bytes: " << file.size() << std::endl;

        file.close();
    }

    sdebug() << "Creating file..." << std::endl;
    write_file<1024 * 1024>(fs, "large.bin");

    sdebug() << "Reading file (256)..." << std::endl;
    read_file<256>(fs, "large.bin");

    sdebug() << "Reading file (512)..." << std::endl;
    read_file<512>(fs, "large.bin");

    sdebug() << "Seek end of file..." << std::endl;
    seek_end_file(fs, "large.bin");

    sdebug() << "Done: " << free_memory() << std::endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
