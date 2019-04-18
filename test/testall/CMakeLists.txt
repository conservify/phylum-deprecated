include(dependencies.cmake)

set(CMAKE_BUILD_TYPE DEBUG)
set(PHYLUM_SRC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(PHYLUM_LINUX_MEMORY_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/linux_memory)
set(PHYLUM_ARDUINO_SERIAL_FLASH_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/arduino_serial_flash)
set(ALOGGING_SRCS "${arduino-logging_PATH}/src")

file(GLOB SRCS *.cpp ${PHYLUM_SRC_DIRECTORY}/*.cpp ${PHYLUM_SRC_DIRECTORY}/phylum/*.h
  ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.cpp ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.h
  ${PHYLUM_ARDUINO_SERIAL_FLASH_BACKEND_SRCS}/*allocator*.cpp ${ALOGGING_SRCS}/*.cpp)

set(PROJECT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PHYLUM_SRC_DIRECTORY}
  ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}
  ${ALOGGING_SRCS}
)

add_definitions("-DPHYLUM_DEBUG=0")

add_executable(testall ${SRCS})
target_include_directories(testall PUBLIC "${PROJECT_INCLUDES}")
target_compile_options(testall PUBLIC -Wall -pedantic)
target_link_libraries(testall libgtest libgmock)
set_target_properties(testall PROPERTIES C_STANDARD 11)
set_target_properties(testall PROPERTIES CXX_STANDARD 11)
add_test(NAME testall-ff COMMAND testall --erase-ff)
add_test(NAME testall-00 COMMAND testall --erase-00)
