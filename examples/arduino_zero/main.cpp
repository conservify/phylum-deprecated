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

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }

    Serial.println("phylum-test: Starting...");

    Serial.println("phylum-test: Initialize Backend");

    Geometry g{ 0, 4, 4, SectorSize };
    ArduinoSdBackend storage;
    if (!storage.initialize(g, 12)) {
        fail();
    }

    Serial.println("phylum-test: Initialize FS");

    SequentialBlockAllocator allocator{ storage };
    FileSystem fs{ storage, allocator };
    if (!fs.initialize(true)) {
        fail();
    }

    Serial.println("phylum-test: Creating small file...");
    {
        auto file = fs.open("small.bin");
        if (!file.write("Jacob", 5)) {
            fail();
        }

        Serial.println("phylum-test: Closing");
        file.close();
    }

    Serial.println("phylum-test: Creating large file...");
    {
        auto file = fs.open("large.bin");
        auto wrote = 0;
        while (wrote < 1024 * 1025) {
            if (file.write("Jacob", 5) != 5) {
                fail();
            }
            wrote += 5;
        }

        Serial.println("phylum-test: Closing");
        file.close();
    }

    Serial.println("phylum-test: Done");

    while (true) {
        delay(10);
    }
}

void loop() {
}
