#ifndef __PHYLUM_PLATFORM_H_INCLUDED
#define __PHYLUM_PLATFORM_H_INCLUDED

#include <alogging/alogging.h>

namespace phylum {

inline ostreamtype phylog() {
    return LogStream{ "Phylum" };
}

}

#endif
