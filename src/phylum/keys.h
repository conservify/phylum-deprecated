#ifndef __PHYLUM_KEYS_H_INCLUDED
#define __PHYLUM_KEYS_H_INCLUDED

namespace phylum {

class Keys {
public:
    // Returns the position where 'key' should be inserted in a leaf node
    // that has the given keys.
    // NOTE: These and the following methods do a simple linear search, which is
    // just fine for N or M < 100. Any large and a Binary Search is better.
    template<typename KEY>
    static unsigned leaf_position_for(const KEY &key, const KEY *keys, unsigned number_keys) {
        uint8_t k = 0;
        while ((k < number_keys) && (keys[k] < key)) {
            ++k;
        }
        assert(k <= number_keys);
        return k;
    }

    // Returns the position where 'key' should be inserted in an inner node
    // that has the given keys.
    template<typename KEY>
    static inline uint8_t inner_position_for(const KEY &key, const KEY *keys, unsigned number_keys) {
        uint8_t k = 0;
        while ((k < number_keys) && ((keys[k] < key) || (keys[k] == key))) {
            ++k;
        }
        return k;
    }

};

}

#endif
