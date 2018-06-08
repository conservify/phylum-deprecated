#ifndef __PHYLUM_CRC_H_INCLUDED
#define __PHYLUM_CRC_H_INCLUDED

#include <cstdint>
#include <cstdlib>

namespace phylum {

uint32_t crc32_update(uint32_t crc, uint8_t data);

uint32_t crc32_checksum(uint8_t *data, size_t size);

}

#endif
