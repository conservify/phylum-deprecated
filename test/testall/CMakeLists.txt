set(CMAKE_BUILD_TYPE DEBUG)
set(PHYLUM_SRC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(PHYLUM_LINUX_MEMORY_BACKEND_SRCS ${PHYLUM_SRC_DIRECTORY}/backends/linux_memory)

file(GLOB SRCS *.cpp ${PHYLUM_SRC_DIRECTORY}/*.cpp ${PHYLUM_SRC_DIRECTORY}/phylum/*.h
                     ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.cpp ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}/*.h)

set(PROJECT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${PHYLUM_SRC_DIRECTORY}
  ${PHYLUM_LINUX_MEMORY_BACKEND_SRCS}
)

#set(PNGPP_SOURCE_DIR ${CMAKE_CURRENT_BINARY_DIR}/png++)
#ExternalProject_Add(
#   png++
#   URL http://download.savannah.nongnu.org/releases/pngpp/png++-0.2.9.tar.gz
#   PREFIX ${PNGPP_SOURCE_DIR}
#   INSTALL_COMMAND ""
#   CONFIGURE_COMMAND ""
#   BUILD_COMMAND ""
#)

# ExternalProject_Get_Property(png++ source_dir)
# set(PNGPP_INCLUDE_DIR "${source_dir}/../")

# find_package(PNG REQUIRED)

# add_definitions("-DPHYLUM_DEBUG=3")
add_executable(testall ${SRCS})
target_include_directories(testall PUBLIC "${PROJECT_INCLUDES}")
target_compile_options(testall PUBLIC -Wall -pedantic)
target_link_libraries(testall libgtest libgmock)
set_target_properties(testall PROPERTIES C_STANDARD 11)
set_target_properties(testall PROPERTIES CXX_STANDARD 11)
add_test(NAME testall-ff COMMAND testall --erase-ff)
add_test(NAME testall-00 COMMAND testall --erase-00)

# target_include_directories(testall PUBLIC ${PNG_INCLUDE_DIR} ${PNGPP_INCLUDE_DIR})
# target_link_libraries(testall ${PNG_LIBRARY})
