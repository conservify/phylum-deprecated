#ifndef __PHYLUM_VISITOR_H_INCLUDED
#define __PHYLUM_VISITOR_H_INCLUDED

#include "phylum/private.h"

namespace phylum {

struct VisitInfo {
    block_index_t block;
    uint32_t position_in_file;
};

class BlockVisitor {
public:
    virtual void block(VisitInfo info) = 0;

};

}

#endif
