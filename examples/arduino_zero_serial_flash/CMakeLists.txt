include(Arduino)

enable_arduino_toolchain()

file(GLOB sources *.cpp ../../src/*.cpp ../../src/phylum/*.h ../../src/backends/arduino_serial_flash/*.cpp)
add_arduino_library(phylum-arduino-zero-serial-flash "${sources}")

find_package(arduino-logging)
target_link_libraries(phylum-arduino-zero-serial-flash arduino-logging)

find_package(SerialFlash)
target_link_libraries(phylum-arduino-zero-serial-flash SerialFlash)

find_package(SPI)
target_link_libraries(phylum-arduino-zero-serial-flash SPI)

add_definitions(-DPHYLUM_ARDUINO_DEBUG=1)
add_definitions(-DPHYLUM_ENABLE_SERIAL_FLASH)

target_include_directories(phylum-arduino-zero-serial-flash PUBLIC ./ ../../src ../../src/backends/arduino_serial_flash)

add_arduino_firmware(phylum-arduino-zero-serial-flash)
