include(Arduino)

enable_arduino_toolchain()

file(GLOB sources *.cpp ../../src/*.cpp ../../src/phylum/*.h ../../src/backends/arduino_sd/*.cpp)
add_arduino_library(phylum-arduino-zero-tree "${sources}")

find_package(arduino-logging)
target_link_libraries(phylum-arduino-zero-tree arduino-logging)

find_package(SPI)
target_link_libraries(phylum-arduino-zero-tree SPI)

add_definitions(-DPHYLUM_DEBUG)
add_definitions(-DPHYLUM_ENABLE_SD)

target_include_directories(phylum-arduino-zero-tree PUBLIC ./ ../../src ../../src/backends/arduino_sd)

add_arduino_firmware(phylum-arduino-zero-tree)
