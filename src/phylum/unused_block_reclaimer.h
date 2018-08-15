#ifndef __PHYLUM_UNUSED_BLOCK_RECLAIMER_H_INCLUDED
#define __PHYLUM_UNUSED_BLOCK_RECLAIMER_H_INCLUDED

#include "phylum/files.h"

namespace phylum {

class UnusedBlockReclaimer {
private:
    Files *files_;
    WanderingBlockManager *wandering_;
    TakenBlockTracker tracker_;

public:
    UnusedBlockReclaimer(Files *files, WanderingBlockManager *wandering) :
        files_(files), wandering_(wandering) {
    }

public:
    bool walk(BlockAddress address);
    bool reclaim();

};

}

#endif
