#include "phylum/phylum.h"

namespace phylum {

#ifdef ARDUINO

phylum::LogStream log;

std::ostream &clog = log;

#else

std::ostream &clog = std::cout;

#endif

}
