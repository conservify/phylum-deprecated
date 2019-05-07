include(Arduino)

enable_arduino_toolchain()

file(GLOB sources *.cpp ../../src/*.cpp ../../src/phylum/*.h
  ../../src/backends/arduino_sd/*.cpp ../../src/backends/arduino_serial_flash/*.cpp)

add_arduino_library(phylum-mcu-test "${sources}")

find_package(arduino-logging)
target_link_libraries(phylum-mcu-test arduino-logging)

find_package(SPI)
target_link_libraries(phylum-mcu-test SPI)

add_definitions(-DPHYLUM_DEBUG=0)

target_include_directories(phylum-mcu-test PUBLIC ./ ../../src
  ../../src/backends/arduino_sd ../../src/backends/arduino_serial_flash)

target_compile_options(phylum-mcu-test PUBLIC -Wall -Werror)

add_arduino_firmware(phylum-mcu-test)
