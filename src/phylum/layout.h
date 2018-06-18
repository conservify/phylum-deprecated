#ifndef __PHYLUM_LAYOUT_H_INCLUDED
#define __PHYLUM_LAYOUT_H_INCLUDED

#include "phylum/block_alloc.h"
#include "phylum/private.h"

namespace phylum {

template<typename THead, typename TTail>
class BlockLayout {
private:
    StorageBackend &storage_;
    Allocator &allocator_;
    Geometry &g_;
    BlockAddress address_;
    BlockType type_;

public:
    BlockAddress address() {
        return address_;
    }

    void address(BlockAddress address) {
        address_ = address;
    }

    BlockAddress add(uint32_t delta) {
        address_.add(delta);
        return address_;
    }

public:
    BlockLayout(StorageBackend &storage, Allocator &allocator, BlockAddress address, BlockType type)
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

    template<typename TEntry>
    bool walk(TEntry &entry) {
        if (should_write_tail(sizeof(TEntry))) {
            auto tl = BlockAddress::tail_data_of(address_.block, g_, sizeof(TTail));
            TTail tail;

            #ifdef PHYLUM_LAYOUT_DEBUG
            sdebug() << "layout: ReadTail: " << tl << " " << sizeof(TTail) << std::endl;
            #endif

            if (!storage_.read(tl, &tail, sizeof(TTail))) {
                return false;
            }

            if (is_valid_block(tail.block.linked_block)) {
                address_ = { tail.block.linked_block, 0 };
            }
            else {
                return false;
            }
        }

        if (address_.beginning_of_block()) {
            if (!verify_head(address_)) {
                return false;
            }
            address_.add(SectorSize);
        }

        assert(address_.find_room(g_, sizeof(TEntry)));

        if (!storage_.read(address_, &entry, sizeof(TEntry))) {
            return false;
        }

        address_.add(sizeof(TEntry));

        return entry.valid();
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
    bool find_append_location(block_index_t block) {
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

        auto end = walk_to_end(block, sizeof(TEntry), fn);
        if (!end.available.valid()) {
            return false;
        }

        address_ = end.available;

        return true;
    }

    template<typename TEntry>
    bool find_tail_entry(block_index_t block) {
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

        return find_tail_entry(block, sizeof(TEntry), fn);
    }

    template<typename TRead>
    bool find_tail_entry(block_index_t block, size_t required, TRead fn) {
        auto end = walk_to_end<TRead>(block, required, fn);
        if (!end.entry.valid()) {
            return false;
        }

        address_ = end.entry;

        return true;
    }

    bool write_head(block_index_t block, block_index_t linked = BLOCK_INDEX_INVALID) {
        auto address = BlockAddress{ block, 0 };

        assert(type_ != BlockType::Error);

        THead head(type_);
        head.fill();
        head.block.linked_block = linked;

        if (!storage_.erase(block)) {
            return false;
        }

        #ifdef PHYLUM_LAYOUT_DEBUG
        sdebug() << "layout: WriteHead: " << address << " " << sizeof(THead) << std::endl;
        #endif
        if (!storage_.write(address, &head, sizeof(THead))) {
            return false;
        }

        return true;
    }

private:
    struct EndOfChain {
        BlockAddress available;
        BlockAddress entry;

        operator bool() {
            return entry.valid() || available.valid();
        }
    };

    bool verify_head(BlockAddress address) {
        THead head(BlockType::Error);
        #ifdef PHYLUM_LAYOUT_DEBUG
        sdebug() << "layout: ReadHead: " << location << " " << sizeof(THead) << std::endl;
        #endif
        if (!storage_.read(address, &head, sizeof(THead))) {
            return false;
        }

        // When we start a new block we always write the head.
        if (!head.valid()) {
            return false;
        }

        return true;
    }

    template<typename TRead>
    EndOfChain walk_to_end(block_index_t block, size_t required, TRead fn) {
        auto verify_block_head = true;
        auto location = BlockAddress{ block, 0 };
        auto found = BlockAddress{ };
        while (location.remaining_in_block(g_) /* - sizeof(TTail)*/ >= required) {
            if (location.beginning_of_block()) {
                if (verify_block_head) {
                    if (!verify_head(location)) {
                        return { };
                    }
                }

                auto tl = BlockAddress::tail_data_of(location.block, g_, sizeof(TTail));
                TTail tail;

                #ifdef PHYLUM_LAYOUT_DEBUG
                sdebug() << "layout: ReadTail: " << tl << " " << sizeof(TTail) << std::endl;
                #endif
                if (!storage_.read(tl, &tail, sizeof(TTail))) {
                    return { };
                }

                if (is_valid_block(tail.block.linked_block)) {
                    location = { tail.block.linked_block, 0 };
                }
                else {
                    location.add(SectorSize);
                }
            }
            else {
                assert(location.find_room(g_, required));

                if (!fn(storage_, location)) {
                    return { location, found };
                }
                else {
                    found = location;
                }

                location.add(required);
            }
        }

        return { };
    }

    bool write_tail(block_index_t block, block_index_t linked) {
        auto address = BlockAddress::tail_data_of(block, g_, sizeof(TTail));

        TTail tail;
        tail.block.linked_block = linked;

        #ifdef PHYLUM_LAYOUT_DEBUG
        sdebug() << "layout: WriteTail: " << address << " " << sizeof(TTail) << std::endl;
        #endif
        if (!storage_.write(address, &tail, sizeof(TTail))) {
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
