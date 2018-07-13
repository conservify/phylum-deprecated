#ifndef __PHYLUM_ARDUINO_SERIAL_FLASH_H_INCLUDED
#define __PHYLUM_ARDUINO_SERIAL_FLASH_H_INCLUDED

#ifdef ARDUINO

#include <SerialFlash.h>

#include <phylum/phylum.h>
#include <phylum/private.h>
#include <phylum/backend.h>

namespace phylum {

class StorageBackendCallbacks {
public:
    virtual bool busy(uint32_t elapsed) = 0;

};

class NoopStorageBackendCallbacks : public StorageBackendCallbacks {
public:
    bool busy(uint32_t elapsed) override {
        return true;
    }

};

class ArduinoSerialFlashBackend : public StorageBackend {
public:
    static constexpr size_t MaximumBlocks = 64;

private:
    StorageBackendCallbacks *callbacks_;
    SerialFlashChip serial_flash_;
    Geometry geometry_;

public:
    ArduinoSerialFlashBackend(StorageBackendCallbacks &callbacks);

public:
    bool initialize(uint8_t cs, sector_index_t sector_size = 512);

public:
    bool open() override;
    bool close() override;
    Geometry &geometry() override;
    bool erase(block_index_t block) override;
    bool read(BlockAddress addr, void *d, size_t n) override;
    bool write(BlockAddress addr, void *d, size_t n) override;
    bool erase();

};

}

#endif // ARDUINO

#endif // __PHYLUM_ARDUINO_SERIAL_FLASH_H_INCLUDED
