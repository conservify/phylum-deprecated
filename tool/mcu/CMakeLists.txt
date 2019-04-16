include(Arduino)

enable_arduino_toolchain()

file(GLOB sources *.cpp ../../src/*.cpp ../../src/phylum/*.h ../../src/backends/arduino_sd/*.cpp)
add_arduino_library(phylum-tool-dump "${sources}")

find_package(arduino-logging)
target_link_libraries(phylum-tool-dump arduino-logging)

find_package(SPI)
target_link_libraries(phylum-tool-dump SPI)

add_definitions(-DPHYLUM_ENABLE_SD)

target_include_directories(phylum-tool-dump PUBLIC ./ ../../src ../../src/backends/arduino_sd)

target_compile_options(phylum-tool-dump PUBLIC -Wall -Werror)

add_arduino_firmware(phylum-tool-dump)
