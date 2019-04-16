cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(dependencies.cmake)

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

add_executable(tool "${sources}")

include_directories(.)
include_directories(../../src)

target_include_directories(tool PUBLIC "${PROJECT_INCLUDES}")

target_compile_options(tool PUBLIC -Wall -pedantic)
