#ifndef __PHYLUM_PLATFORM_H_INCLUDED
#define __PHYLUM_PLATFORM_H_INCLUDED

#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#undef min
#undef max
#else
#undef min
#undef max
#include <string>
#include <ostream>
#endif

namespace phylum {

class LogStream {
public:
    LogStream& print(const char *str) {
        #ifdef ARDUINO
        if (Serial) {
            Serial.print(str);
        }
        if (Serial5) {
            Serial5.print(str);
        }
        #else
        printf("%s", str);
        #endif
        return *this;
    }

    LogStream& printf(const char *f, ...) {
        #ifdef PHYLUM_DISABLE_LOGGING
        return *this;
        #else
        char buffer[128];
        va_list args;
        va_start(args, f);
        vsnprintf(buffer, sizeof(buffer), f, args);
        va_end(args);
        return print(buffer);
        #endif
    }

public:
    LogStream& operator<<(uint8_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(uint16_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(uint32_t i) {
        return printf("%lu", i);
    }

    LogStream& operator<<(uint64_t i) {
        return printf("%lu", i);
    }

    LogStream& operator<<(int8_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(int16_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(int32_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(int64_t i) {
        return printf("%d", i);
    }

    #ifdef ARDUINO // Ick
    LogStream& operator<<(size_t i) {
        return printf("%d", i);
    }
    #else
    LogStream& operator<<(std::string &v) {
        return printf("%s", v.c_str());
    }
    #endif

    LogStream& operator<<(double v) {
        return printf("%f", v);
    }

    LogStream& operator<<(float v) {
        return printf("%f", v);
    }

    LogStream& operator<<(const char c) {
        return printf("%c", c);
    }

    LogStream& operator<<(const char *s) {
        return printf("%s", s);
    }
};

}

#ifdef ARDUINO

namespace phylum {

constexpr char endl = '\n';

using ostreamtype = phylum::LogStream;

}

#else

namespace phylum {

constexpr char endl = '\n';

using ostreamtype = ::std::ostream;

}

#endif

namespace phylum {

extern ostreamtype &clog;

inline ostreamtype &sdebug() {
    return clog;
}

}

#endif
