set(ARDUINO_IDE "${CMAKE_CURRENT_SOURCE_DIR}/../../../arduino-1.8.3")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../../../arduino-cmake-minimal/cmake/")

include(ArduinoMinimal)

set(LIBRARIES SPI)

file(GLOB SOURCE_FILES *.cpp ../../src/*.cpp ../../src/phylum/*.h ../../src/backends/arduino_sd/*.cpp)

arduino(phylum-arduino-zero-prealloc "${SOURCE_FILES}" "${LIBRARIES}")

add_definitions("-DPHYLUM_DEBUG")

target_include_directories(phylum-arduino-zero-prealloc PUBLIC ./ ../../src ../../src/backends/arduino_sd)
