#ifndef __PHYLUM_LAYOUT_H_INCLUDED
#define __PHYLUM_LAYOUT_H_INCLUDED

#include "phylum/block_alloc.h"
#include "phylum/private.h"

namespace phylum {

template<typename THead, typename TTail>
class BlockLayout {
private:
    StorageBackend &storage_;
    BlockAllocator &allocator_;
    Geometry &g_;
    BlockAddress address_;
    BlockType type_;

public:
    BlockAddress address() {
        return address_;
    }

    BlockAddress add(uint32_t delta) {
        address_.add(delta);
        return address_;
    }

public:
    BlockLayout(StorageBackend &storage, BlockAllocator &allocator, BlockAddress address, BlockType type)
        : storage_(storage), allocator_(allocator), g_(storage.geometry()), address_(address), type_(type) {
    }

public:
    template<typename TEntry>
    bool append(TEntry entry) {
        auto address = find_available(sizeof(TEntry));

        if (!address.valid()) {
            return false;
        }

        if (!storage_.write(address, &entry, sizeof(TEntry))) {
            return false;
        }

        return true;
    }

    bool walk_block(size_t required) {
        if (need_new_block() || should_write_tail(required)) {
            return false;
        }

        assert(address_.find_room(g_, required));

        return true;
    }

    BlockAddress find_available(size_t required) {
        if (need_new_block() || should_write_tail(required)) {
            assert(type_ != BlockType::Error);
            auto new_block = allocator_.allocate(type_);
            if (!write_head(new_block, address_.block)) {
                return { };
            }

            // Write or tail, linking us to the following blink.
            if (address_.valid()) {
                if (!write_tail(address_.block, new_block)) {
                    return { };
                }
            }

            address_ = { new_block, SectorSize };
        }

        // If at beginning of a block, append head. This will rarely be true.
        if (should_write_head()) {
            if (!write_head(address_.block)) {
                return { };
            }

            address_.add(SectorSize);
        }

        // We can assert here because we check for the end of the block above,
        // which is the only time that this method should fail.
        assert(address_.find_room(g_, required));

        auto opening = address_;
        address_.add(required);
        return opening;
    }

    template<typename TEntry>
    bool find_end(block_index_t block) {
        auto fn = [](StorageBackend &storage, BlockAddress &address) -> bool {
            TEntry entry;

            if (!storage.read(address, &entry, sizeof(TEntry))) {
                return false;
            }

            if (!entry.valid()) {
                return false;
            }

            return true;
        };

        return find_end(block, sizeof(TEntry), fn);
    }

    template<typename TRead>
    bool find_end(block_index_t block, size_t required, TRead fn) {
        auto location = BlockAddress{ block, 0 };
        while (location.remaining_in_block(g_) > required) {
            if (location.beginning_of_block()) {
                if (true/*veriy head*/) {
                    THead head(BlockType::Error);
                    if (!storage_.read(location, &head, sizeof(THead))) {
                        return { };
                    }

                    if (!head.valid()) {
                        address_ = location;
                        return true;
                    }
                }

                auto tl = BlockAddress::tail_data_of(location.block, g_, sizeof(TTail));
                TTail tail;

                if (!storage_.read(tl, &tail, sizeof(TTail))) {
                    return false;
                }

                if (is_valid_block(tail.linked_block)) {
                    location = { tail.linked_block, 0 };
                }
                else {
                    location.add(SectorSize);
                }
            }
            else {
                assert(location.find_room(g_, required));

                if (!fn(storage_, location)) {
                    address_ = location;
                    return true;
                }

                location.add(required);
            }
        }
        return false;
    }

    bool write_head(block_index_t block, block_index_t linked = BLOCK_INDEX_INVALID) {
        assert(type_ != BlockType::Error);

        THead head(type_);
        head.fill();
        head.header.linked_block = linked;

        if (!storage_.erase(block)) {
            return false;
        }

        if (!storage_.write({ block, 0 }, &head, sizeof(THead))) {
            return false;
        }

        return true;
    }

private:
    bool write_tail(block_index_t block, block_index_t linked) {
        auto tl = BlockAddress::tail_data_of(block, g_, sizeof(TTail));

        TTail tail;
        tail.linked_block = linked;

        if (!storage_.write(tl, &tail, sizeof(TTail))) {
            return false;
        }

        return true;
    }

    bool should_write_head() {
        return address_.beginning_of_block();
    }

    bool should_write_tail(size_t required) {
        auto remaining = address_.remaining_in_block(g_);

        if (remaining >= required + sizeof(TTail)) {
            return false;
        }

        return true;
    }

    bool need_new_block()  {
        return !address_.valid();
    }

};

}

#endif
