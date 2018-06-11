#ifndef __PHYLUM_JOURNAL_H_INCLUDED
#define __PHYLUM_JOURNAL_H_INCLUDED

#include "phylum/backend.h"

namespace phylum {

class Journal {
private:
    StorageBackend *storage_;

public:
    Journal(StorageBackend &storage);

public:
    bool format(block_index_t block);

};

}

#endif
