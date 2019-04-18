cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ExternalProject)

include(dependencies.cmake)

set(PROTOBUF_TAR_GZ https://github.com/google/protobuf/archive/v3.4.0.tar.gz)

ExternalProject_Add(
  protobuf-external
  PREFIX protobuf
  URL ${PROTOBUF_TAR_GZ}
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf
  CMAKE_CACHE_ARGS
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-Dprotobuf_BUILD_TESTS:BOOL=OFF"
    "-Dprotobuf_BUILD_EXAMPLES:BOOL=OFF"
    "-Dprotobuf_WITH_ZLIB:BOOL=OFF"
    "-DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}"
    # other project specific parameters
  SOURCE_SUBDIR cmake
  BUILD_ALWAYS 1
  STEP_TARGETS build
  INSTALL_COMMAND ""
)

set(CMAKE_BUILD_TYPE DEBUG)
set(PHYLUM_SRC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(PHYLUM_LINUX_MEMORY_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/linux_memory)
set(PHYLUM_ARDUINO_SERIAL_FLASH_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/arduino_serial_flash)
set(ALOGGING_SOURCE_DIRECTORY "${arduino-logging_PATH}/src")

file(GLOB sources *.cpp ${PHYLUM_SRC_DIRECTORY}/*.cpp ${PHYLUM_SRC_DIRECTORY}/phylum/*.h
  ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.cpp ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.h
  ${PHYLUM_ARDUINO_SERIAL_FLASH_BACKEND_SRCS}/*allocator*.cpp ${ALOGGING_SOURCE_DIRECTORY}/*.cpp)

set(PROJECT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PHYLUM_SRC_DIRECTORY}
  ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}
  ${ALOGGING_SOURCE_DIRECTORY}
)

add_definitions(-DPHYLUM_READ_ONLY)
add_definitions(-DPHYLUM_DEBUG=0)

# Get GTest source and binary directories from CMake project
ExternalProject_Get_Property(gtest source_dir binary_dir)

ExternalProject_Get_Property(protobuf-external source_dir)
include_directories(${source_dir}/src)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/protobuf)

add_executable(tool ${sources})

include_directories(.)

include_directories(../../src)

target_include_directories(tool PUBLIC "${PROJECT_INCLUDES}")

target_compile_options(tool PUBLIC -Wall -pedantic)

target_link_libraries(tool protobuf)

target_link_libraries(tool stdc++fs)
