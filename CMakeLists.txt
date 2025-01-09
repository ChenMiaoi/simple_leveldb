cmake_minimum_required(VERSION 3.8.0)
project(simple_leveldb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/static")

option(ENABLE_SHARED_LIB_BUILD "enable shared library build" ON)
option(ENABLE_EXAMPLES_BUILD "enable examples build" ON)
option(ENABLE_TESTS_BUILD "enable tests build" ON)

set(SIMPLE_LEVELDB_SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src)
set(SIMPLE_LEVELDB_CORE_HEADER_ROOT ${CMAKE_SOURCE_DIR}/include/core)
set(SIMPLE_LEVELDB_CORE_SOURCE_ROOT ${CMAKE_SOURCE_DIR}/src/core)

file(GLOB_RECURSE CORE_HEADERS ${SIMPLE_LEVELDB_CORE_HEADER_ROOT}/*.h)
file(GLOB_RECURSE CORE_SOURSES ${SIMPLE_LEVELDB_CORE_SOURCE_ROOT}/*.cc)

set(NEED_INCLUDE_DIRS 
  ${CMAKE_SOURCE_DIR}/include
  ${SIMPLE_LEVELDB_CORE_HEADER_ROOT}
)

include_directories(
  ${NEED_INCLUDE_DIRS}
)

add_library(simple_leveldb STATIC
  ${SIMPLE_LEVELDB_SOURCE_ROOT}/db.cc
  ${CORE_SOURSES}
)

target_include_directories(simple_leveldb PRIVATE
  ${NEED_INCLUDE_DIRS}
)

if(ENABLE_EXAMPLES_BUILD)
  add_subdirectory(examples)
endif()

if(ENABLE_TESTS_BUILD)
  include_directories(deps/googletest/googletest/include)
  add_subdirectory(deps/googletest)
  add_subdirectory(tests)
endif()
