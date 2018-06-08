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

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    sdebug() << "phylum-test: Starting: " << free_memory() << std::endl;

    sdebug() << "phylum-test: Done: " << free_memory() << std::endl;
    sdebug() << "phylum-test: sizeof(FileSystem): " << sizeof(FileSystem) << std::endl;
    sdebug() << "phylum-test: sizeof(OpenFile): " << sizeof(OpenFile) << std::endl;
    sdebug() << "phylum-test: sizeof(SequentialBlockAllocator): " << sizeof(SequentialBlockAllocator) << std::endl;
    sdebug() << "phylum-test: sizeof(ArduinoSdBackend): " << sizeof(ArduinoSdBackend) << std::endl;
    sdebug() << "phylum-test: sizeof(Node): " << sizeof(Node<uint64_t, uint64_t, BlockAddress, 6, 6>) << std::endl;

    sdebug() << "phylum-test: Initialize Backend" << std::endl;

    Geometry g{ 0, 4, 4, SectorSize };
    ArduinoSdBackend storage;
    if (!storage.initialize(g, 12)) {
        fail();
    }

    sdebug() << "phylum-test: Initialize FS" << std::endl;

    SequentialBlockAllocator allocator{ storage };
    FileSystem fs{ storage, allocator };
    if (!fs.initialize(true)) {
        fail();
    }

    sdebug() << "phylum-test: Creating small file..." << std::endl;
    {
        auto file = fs.open("small.bin");
        if (!file.write("Jacob", 5)) {
            fail();
        }

        sdebug() << "Bytes: " << file.size() << std::endl;

        file.close();
    }

    sdebug() << "phylum-test: Creating large file..." << std::endl;
    {
        auto file = fs.open("large.bin");
        auto wrote = 0;
        while (wrote < 1024 * 1025) {
            if (file.write("Jacob", 5) != 5) {
                fail();
            }
            wrote += 5;
        }

        sdebug() << "Bytes: " << file.size() << std::endl;

        file.close();
    }

    while (true) {
        delay(10);
    }
}

void loop() {
}
