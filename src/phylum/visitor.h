#ifndef __PHYLUM_VISITOR_H_INCLUDED
#define __PHYLUM_VISITOR_H_INCLUDED

#include "phylum/private.h"

namespace phylum {

class BlockVisitor {
public:
    virtual void block(block_index_t index) = 0;

};

}

#endif
