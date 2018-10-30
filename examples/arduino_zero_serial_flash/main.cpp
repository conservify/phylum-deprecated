#include <Arduino.h>

#include <phylum/phylum.h>
#include <phylum/basic_super_block_manager.h>
#include <backends/arduino_serial_flash/arduino_serial_flash.h>
#include <backends/arduino_serial_flash/serial_flash_allocator.h>

using namespace phylum;

typedef struct board_configuration_t {
    uint8_t periph_enable;
    uint8_t flash_cs;
    uint8_t sd_cs;
    uint8_t wifi_cs;
    uint8_t rfm95_cs;
} board_configuration_t;

board_configuration_t possible_boards[4] = {
    { 25u, 26u, 12u,  7u,  5u }, // Core
    {  8u,  6u,  0u,  0u,  5u }, // Sonar
    { 12u,  5u,  0u,  0u,  0u }, // Atlas
    {  8u,  5u,  0u,  0u,  0u }, // Weather
};

struct SimpleState : MinimumSuperBlock {
};

using OurStateManager = BasicSuperBlockManager<SimpleState>;

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

    sdebug() << "Initialize Storage" << endl;

    NoopStorageBackendCallbacks callbacks;
    ArduinoSerialFlashBackend storage{ callbacks };
    auto success = false;
    for (auto cfg : possible_boards) {
        if (cfg.periph_enable > 0) {
            pinMode(cfg.periph_enable, OUTPUT);
            digitalWrite(cfg.periph_enable, LOW);
            delay(100);
            digitalWrite(cfg.periph_enable, HIGH);
            delay(100);
        }

        if (cfg.rfm95_cs > 0) {
            pinMode(cfg.rfm95_cs, OUTPUT);
            digitalWrite(cfg.rfm95_cs, HIGH);
        }

        if (cfg.sd_cs > 0) {
            pinMode(cfg.sd_cs, OUTPUT);
            digitalWrite(cfg.sd_cs, HIGH);
        }

        if (cfg.wifi_cs > 0) {
            pinMode(cfg.wifi_cs, OUTPUT);
            digitalWrite(cfg.wifi_cs, HIGH);
        }

        if (cfg.flash_cs > 0) {
            pinMode(cfg.flash_cs, OUTPUT);
            digitalWrite(cfg.flash_cs, HIGH);
        }

        if (storage.initialize(cfg.flash_cs)) {
            sdebug() << "Found on #" << cfg.flash_cs << endl;
            success = true;
            break;
        }
    }

    if (!success) {
        fail();
    }

    if (!storage.open()) {
        fail();
    }

    sdebug() << "Initialize Allocator" << endl;

    SerialFlashAllocator allocator{ storage };
    if (!allocator.initialize()) {
        fail();
    }

    OurStateManager sbm{ storage, allocator };

    sdebug() << "Initialize FS" << endl;

    if (!sbm.locate()) {
        sdebug() << "Locate failed, creating..." << endl;

        if (!sbm.create()) {
            sdebug() << "Create failed" << endl;
            fail();
        }

        if (!sbm.locate()) {
            sdebug() << "Locate failed" << endl;
            fail();
        }
    }

    sdebug() << "Ready!" << endl;

    for (auto i = 0; i < 1024; ++i) {
        if (!sbm.save()) {
            sdebug() << "Save failed!" << endl;
            fail();
        }

        sdebug() << "New Location: " << sbm.location() << endl;
    }

    sdebug() << "DONE!" << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
