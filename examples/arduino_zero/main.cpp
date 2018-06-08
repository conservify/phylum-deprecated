#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>

using namespace phylum;

static ArduinoSdBackend backend;

void setup() {
    Serial.begin(115200);

    while (!Serial) {
        delay(10);
    }
}

void loop() {
}
