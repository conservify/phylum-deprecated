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

#endif
