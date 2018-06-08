set(CMAKE_BUILD_TYPE DEBUG)
set(PHYLUM_SRC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(PHYLUM_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/linux-memory)

file(GLOB SRCS *.cpp ${PHYLUM_SRC_DIRECTORY}/*.cpp ${PHYLUM_SRC_DIRECTORY}/phylum/*.h ${PHYLUM_BACKEND_SRCS}/*.cpp ${PHYLUM_BACKEND_SRCS}/*.h)

set(PROJECT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PHYLUM_SRC_DIRECTORY}
  ${PHYLUM_BACKEND_SRCS}
)

add_executable(testall ${SRCS})
target_include_directories(testall PUBLIC "${PROJECT_INCLUDES}")
target_compile_options(testall PUBLIC -Wall -pedantic)
target_link_libraries(testall libgtest libgmock)
set_target_properties(testall PROPERTIES C_STANDARD 11)
set_target_properties(testall PROPERTIES CXX_STANDARD 11)
add_test(NAME testall-ff COMMAND testall --ff)
add_test(NAME testall-00 COMMAND testall --00)
