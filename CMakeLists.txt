cmake_minimum_required(VERSION 3.5)
project(phylum)

set(CMAKE_MODULE_PATH $ENV{CMAKE_MODULE_PATH})

enable_testing()

option(DEBUG_LOGGING "Enable debug logging")
if(DEBUG_LOGGING)
  add_definitions(-DPHYLUM_DEBUG)
endif(DEBUG_LOGGING)

option(DEBUG_LOGGING "Enable debug layout logging")
if(DEBUG_LAYOUT_LOGGING)
  add_definitions(-DPHYLUM_LAYOUT_DEBUG)
endif(DEBUG_LAYOUT_LOGGING)

add_subdirectory(test)
add_subdirectory(examples)
add_subdirectory(tool)
