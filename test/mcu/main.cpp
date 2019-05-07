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

    while (!Serial) {
        delay(10);
    }

    Log::info("Free Memory: %lu", free_memory());

    auto board = &sonar_board;

    board->disable_everything();
    delay(100);
    board->enable_everything();
    delay(100);

    Log::info("Ready");

    NoopStorageBackendCallbacks callbacks;
    ArduinoSerialFlashBackend storage{ callbacks };

    if (!storage.initialize(board->flash_cs(), 2048)) {
        Log::error("Error initializing Flash");
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
