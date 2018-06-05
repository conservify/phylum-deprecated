set(CMAKE_BUILD_TYPE DEBUG)
set(CONFS_SRC_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../../src)
set(CONFS_BACKEND_SRCS ${CONFS_SRC_DIRECTORY}/backends/linux-memory)

file(GLOB SRCS *.cpp ${CONFS_SRC_DIRECTORY}/*.cpp ${CONFS_SRC_DIRECTORY}/confs/*.h ${CONFS_BACKEND_SRCS}/*.cpp ${CONFS_BACKEND_SRCS}/*.h)

set(PROJECT_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CONFS_SRC_DIRECTORY}
  ${CONFS_BACKEND_SRCS}
)

add_executable(testall ${SRCS})
target_include_directories(testall PUBLIC "${PROJECT_INCLUDES}")
target_compile_options(testall PUBLIC -Wall -pedantic)
target_link_libraries(testall libgtest libgmock)
set_target_properties(testall PROPERTIES C_STANDARD 11)
set_target_properties(testall PROPERTIES CXX_STANDARD 11)
add_test(NAME testall-ff COMMAND testall --ff)
add_test(NAME testall-00 COMMAND testall --00)
