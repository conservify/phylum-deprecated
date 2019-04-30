#include "phylum_input_stream.h"

using namespace google::protobuf;
using namespace google::protobuf::io;

using namespace phylum;

PhylumInputStream::PhylumInputStream(Geometry geometry, uint8_t *everything, block_index_t block)
    : geometry_(geometry), address_{ block, 0 }, everything_(everything), iter_(nullptr), position_(0), sector_remaining_(0) {
}

bool PhylumInputStream::Next(const void **data, int *size) {
    auto &g = geometry_;

    *data = nullptr;
    *size = 0;

    while (true) {
        if (sector_remaining_ == 0) {
            auto follow = address_.tail_sector(g);

            address_.position += address_.remaining_in_sector(g);

            auto sector = address_.sector_number(g);

            if (address_.tail_sector(g)) {
                auto offset = ((uint64_t)address_.block * g.block_size()) + (SectorSize * g.sectors_per_block());
                auto &sector_tail = *(FileBlockTail *)(everything_ + offset - sizeof(FileBlockTail));

                if (follow) {
                    address_ = BlockAddress{ sector_tail.block.linked_block, 0 };
                    sector_remaining_ = 0;
                    continue;
                }

                sector_remaining_ = sector_tail.sector.bytes;
            }
            else {
                auto offset = ((uint64_t)address_.block * g.block_size()) + (SectorSize * (sector + 1));
                auto &sector_tail = *(FileSectorTail *)(everything_ + offset - sizeof(FileSectorTail));
                sector_remaining_ = sector_tail.bytes;
            }

            iter_ = everything_ + ((uint64_t)address_.block * g.block_size()) + address_.position;

            if (sector_remaining_ == 0) {
                return false;
            }
        }

        break;
    }

    previous_block_ = Block{ iter_, sector_remaining_ };

    *data = previous_block_.ptr;
    *size = previous_block_.size;

    position_ += sector_remaining_;
    sector_remaining_ = 0;

    return true;
}

void PhylumInputStream::BackUp(int c) {
    sector_remaining_ = c;
    iter_ = previous_block_.ptr + (previous_block_.size - c);
    position_ -= c;
}

bool PhylumInputStream::Skip(int c) {
    assert(false);
    return true;
}

int64 PhylumInputStream::ByteCount() const {
    assert(false);
    return 0;
}

