include(ExternalProject)

set(PROTOBUF_TAR_GZ https://github.com/google/protobuf/archive/v3.4.0.tar.gz)

ExternalProject_Add(
  protobuf-external
  PREFIX protobuf
  URL ${PROTOBUF_TAR_GZ}
  BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/protobuf
  CMAKE_CACHE_ARGS
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-Dprotobuf_BUILD_TESTS:BOOL=OFF"
    "-Dprotobuf_BUILD_EXAMPLES:BOOL=OFF"
    "-Dprotobuf_WITH_ZLIB:BOOL=OFF"
    "-DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}"
    # other project specific parameters
  SOURCE_SUBDIR cmake
  BUILD_ALWAYS 1
  STEP_TARGETS build
  INSTALL_COMMAND ""
)

# Create a libprotobuf target to be used as a dependency by test programs
ExternalProject_Get_Property(protobuf-external source_dir binary_dir)

add_library(libprotobuf IMPORTED STATIC GLOBAL)
add_dependencies(libprotobuf protobuf-external)

set_target_properties(libprotobuf PROPERTIES
  "IMPORTED_LOCATION" "${binary_dir}/libprotobuf.a"
  "IMPORTED_LINK_INTERFACE_LIBRARIES" "${CMAKE_THREAD_LIBS_INIT}"
)

include_directories(${source_dir}/src)
link_directories(${CMAKE_CURRENT_BINARY_DIR}/protobuf)

add_subdirectory(linux)
add_subdirectory(mcu)
