#ifndef __PHYLUM_PLATFORM_H_INCLUDED
#define __PHYLUM_PLATFORM_H_INCLUDED

#include <cstdint>
#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#else
#include <iostream>
#include <string>
#endif

namespace phylum {

class LogStream {
public:
    LogStream& print(const char *str) {
        #ifdef ARDUINO
        Serial.print(str);
        #else
        printf("%s", str);
        #endif
        return *this;
    }

    LogStream& printf(const char *f, ...) {
        char buffer[128];
        va_list args;
        va_start(args, f);
        vsnprintf(buffer, sizeof(buffer), f, args);
        va_end(args);
        return print(buffer);
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

    LogStream& operator<<(int8_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(int16_t i) {
        return printf("%d", i);
    }

    LogStream& operator<<(int32_t i) {
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
namespace std {

/**
 *
 */
using ostream = phylum::LogStream;

/**
 *
 */
constexpr char endl = '\n';

}

#else

#endif

namespace phylum {

extern std::ostream &clog;

inline std::ostream &sdebug() {
    return clog;
}

}

#endif
