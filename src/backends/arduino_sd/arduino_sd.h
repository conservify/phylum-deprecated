#ifndef __PHYLUM_ARDUINO_SD_H_INCLUDED
#define __PHYLUM_ARDUINO_SD_H_INCLUDED

#ifdef PHYLUM_ENABLE_SD
#ifdef ARDUINO

#include <phylum/phylum.h>
#include <phylum/private.h>
#include <phylum/backend.h>

#include "sd_raw.h"

namespace phylum {

class ArduinoSdBackend : public StorageBackend {
private:
    sd_raw_t sd_;
    Geometry geometry_;

public:
    ArduinoSdBackend();

public:
    bool initialize(const Geometry &g, uint8_t sd);

public:
    bool open() override;
    bool close() override;
    Geometry &geometry() override;
    bool erase(block_index_t block) override;
    bool read(BlockAddress addr, void *d, size_t n) override;
    bool write(BlockAddress addr, void *d, size_t n) override;

};

}

#endif // ARDUINO
#endif // PHYLUM_ENABLE_SD

#endif // __PHYLUM_ARDUINO_SD_H_INCLUDED
