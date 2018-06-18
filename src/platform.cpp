#include "phylum/phylum.h"

namespace phylum {

#ifdef ARDUINO

phylum::LogStream log;

ostreamtype &clog = log;

#else

ostreamtype &clog = std::cout;

#endif

}
