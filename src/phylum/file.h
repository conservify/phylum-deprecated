#ifndef __PHYLUM_FILE_H_INCLUDED
#define __PHYLUM_FILE_H_INCLUDED

namespace phylum {

class File {
public:
    virtual BlockAddress beginning() const = 0;
    virtual uint32_t version() const = 0;
    virtual uint64_t size() const = 0;
    virtual uint64_t tell() const = 0;
    virtual bool seek(uint64_t position) = 0;
    virtual int32_t read(uint8_t *ptr, size_t size) = 0;
    virtual int32_t write(uint8_t *ptr, size_t size, bool span_sectors = true, bool span_blocks = true) = 0;
    virtual void close() = 0;

};

}

#endif
