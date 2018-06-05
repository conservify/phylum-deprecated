#ifndef __CONFS_UTILITIES_H_INCLUDED
#define __CONFS_UTILITIES_H_INCLUDED

#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <iomanip>
#include <string>

#include <confs/tree.h>
#include <confs/private.h>

using namespace confs;

typedef int64_t btree_value_t;

std::map<btree_key_t, btree_value_t> random_data();

inline uint64_t make_key(uint32_t inode, uint32_t offset) {
    return ((uint64_t)offset << 32) | (uint64_t)inode;
}

#endif
