#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>
#include <backends/arduino_serial_flash/arduino_serial_flash.h>

#include "boards.h"

static uint32_t free_memory();

using namespace phylum;

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;

void setup() {
    Serial.begin(115200);

    Log::info("Free Memory: %lu", free_memory());

    while (!Serial) {
        delay(10);
    }
}

void loop() {
    delay(10);
}

extern "C" char *sbrk(int32_t i);

static uint32_t free_memory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}
