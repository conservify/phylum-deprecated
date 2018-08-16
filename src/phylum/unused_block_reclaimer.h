#ifndef __PHYLUM_UNUSED_BLOCK_RECLAIMER_H_INCLUDED
#define __PHYLUM_UNUSED_BLOCK_RECLAIMER_H_INCLUDED

#include "phylum/files.h"

namespace phylum {

class UnusedBlockReclaimer {
private:
    Files *files_;
    SuperBlockManager *sbm_;
    TakenBlockTracker tracker_;

public:
    UnusedBlockReclaimer(Files &files, SuperBlockManager &sbm) :
        files_(&files), sbm_(&sbm) {
    }

public:
    bool walk(BlockAddress address);
    bool reclaim();

};

}

#endif
