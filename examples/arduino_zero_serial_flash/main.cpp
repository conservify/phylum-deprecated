#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_serial_flash/arduino_serial_flash.h>
#include <backends/arduino_serial_flash/serial_flash_allocator.h>

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


    sdebug() << "Initialize Storage" << endl;

    ArduinoSerialFlashBackend storage;
    if (!storage.initialize(4)) {
        fail();
    }

    if (!storage.open()) {
        fail();
    }

    sdebug() << "Initialize Allocator" << endl;

    SerialFlashAllocator allocator{ storage };
    if (!allocator.initialize(storage.geometry())) {
        fail();
    }

    SuperBlockManager sbm{ storage, allocator };

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

    for (auto i = 0; i < 128; ++i) {
        if (!sbm.save()) {
            fail();
        }
    }

    while (true) {
        delay(10);
    }
}

void loop() {
}
