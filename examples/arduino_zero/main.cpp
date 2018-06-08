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

    FileSystem fs{ storage };
    if (!fs.initialize(true)) {
        fail();
    }

    Serial.println("phylum-test: Done");

    while (true) {
        delay(10);
    }
}

void loop() {
}
