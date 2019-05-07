#ifndef __PHYLUM_MAGIC_H_INCLUDED
#define __PHYLUM_MAGIC_H_INCLUDED

namespace phylum {

struct BlockMagic {
    static constexpr char MagicKey[] = "phylum00";

    char key[sizeof(MagicKey)] = { 0 };

    BlockMagic() {
    }

    BlockMagic(const char *k);

    static BlockMagic get_valid() {
        return { MagicKey };
    }

    void fill();
    bool valid() const;
};

}

#endif // __PHYLUM_MAGIC_H_INCLUDED
