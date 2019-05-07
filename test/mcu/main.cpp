#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>
#include <backends/arduino_serial_flash/arduino_serial_flash.h>

#include "boards.h"

static uint32_t free_memory();

using namespace phylum;

constexpr const char LogName[] = "Phylum";

using Log = SimpleLog<LogName>;

bool read_all_blocks(StorageBackend &storage, block_index_t desired_reads, size_t size) {
    auto g = storage.geometry();

    auto started = millis();

    uint8_t buffer[size];

    for (auto b = (block_index_t)0; b < desired_reads; ++b) {
        auto address = BlockAddress{ b % g.number_of_blocks, 0 };
        if (!storage.read(address, buffer, sizeof(buffer))) {
            Log::error("Error reading block %lu", address.block);
            return false;
        }
    }

    auto ended = millis();

    auto blocks_per_second = (float)desired_reads / ((float)(ended - started) / 1000.0f);

    Log::info("Reading %5d bytes from %5lu blocks in %8lums (%f blocks/sec)",
              size, desired_reads, ended - started, blocks_per_second);

    return true;
}

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

    NoopStorageBackendCallbacks callbacks;
    ArduinoSerialFlashBackend storage{ callbacks };

    if (!storage.initialize(board->flash_cs(), 2048, 0)) {
        Log::error("Error initializing Flash");
        return;
    }

    if (true) {
        for (auto i = 1; i <= 4; ++i) {
            assert(read_all_blocks(storage, 2048 * i, 32));
            assert(read_all_blocks(storage, 2048 * i, 64));
            assert(read_all_blocks(storage, 2048 * i, 128));
            assert(read_all_blocks(storage, 2048 * i, 256));
            assert(read_all_blocks(storage, 2048 * i, 2048));
        }
    }

    Log::info("Done!");
}

void loop() {
    delay(10);
}

extern "C" char *sbrk(int32_t i);

static uint32_t free_memory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}
