include(Arduino)

enable_arduino_toolchain()

file(GLOB sources *.cpp ../../src/*.cpp ../../src/phylum/*.h ../../src/backends/arduino_sd/*.cpp)
add_arduino_library(phylum-arduino-zero-large-files "${sources}")

find_package(arduino-logging)
target_link_libraries(phylum-arduino-zero-large-files arduino-logging)

find_package(SPI)
target_link_libraries(phylum-arduino-zero-large-files SPI)

add_definitions(-DPHYLUM_ENABLE_SD)

target_include_directories(phylum-arduino-zero-large-files PUBLIC ./ ../../src ../../src/backends/arduino_sd)

target_compile_options(phylum-arduino-zero-large-files PUBLIC -Wall -Werror)

add_arduino_firmware(phylum-arduino-zero-large-files)
