#include "arduino_sd.h"

namespace phylum {

ArduinoSdBackend::ArduinoSdBackend() {
}

bool ArduinoSdBackend::initialize() {
    return true;
}

bool ArduinoSdBackend::open() {
    return true;
}

bool ArduinoSdBackend::close() {
    return true;
}

Geometry &ArduinoSdBackend::geometry() {
}

size_t ArduinoSdBackend::size() {
    return 0;
}

bool ArduinoSdBackend::erase(block_index_t block) {
    return true;
}

void ArduinoSdBackend::randomize() {
}

bool ArduinoSdBackend::read(SectorAddress addr, size_t offset, void *d, size_t n) {
    return true;
}

bool ArduinoSdBackend::write(SectorAddress addr, size_t offset, void *d, size_t n) {
    return true;
}

bool ArduinoSdBackend::read(BlockAddress addr, void *d, size_t n) {
    return true;
}

bool ArduinoSdBackend::write(BlockAddress addr, void *d, size_t n) {
    return true;
}

}
