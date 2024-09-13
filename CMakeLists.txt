cmake_minimum_required(VERSION 3.8.0)
project(simple_leveldb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true) 

# set(CMAKE_CXX_COMPILE_FLAGS "-Werror -O2 -g -pedantic -Wconversion -Wfloat-conversion -Wsign-conversion")

if (WIN32)
  set(SIMPLE_LEVELDB_PLATFORM_NAME SIMPLE_LEVELDB_PLATFORM_WINDOWS)
  # TODO(cmumford): Make UNICODE configurable for Windows.
  add_definitions(-D_UNICODE -DUNICODE)
else (WIN32)
  set(SIMPLE_LEVELDB_PLATFORM_NAME SIMPLE_LEVELDB_PLATFORM_POSIX)
endif (WIN32)

file(GLOB_RECURSE  LEVELDB_HEADERS ${CMAKE_SOURCE_DIR}/include/leveldb/*.h)
file(GLOB PORT_HEADERS ${CMAKE_SOURCE_DIR}/include/port/*.h)
file(GLOB TABLE_HEADERS ${CMAKE_SOURCE_DIR}/include/table/*.h)
file(GLOB UTIL_HEADERS ${CMAKE_SOURCE_DIR}/include/util/*.h)
file(GLOB_RECURSE  LEVELDB_SOURCES ${CMAKE_SOURCE_DIR}/src/leveldb/*.cc)
file(GLOB PORT_SOURCES ${CMAKE_SOURCE_DIR}/src/port/*.cc)
file(GLOB TABLE_SOURCES ${CMAKE_SOURCE_DIR}/src/table/*.cc)
file(GLOB UTIL_SOURCES ${CMAKE_SOURCE_DIR}/src/util/*.cc)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SIMPLE_LEVELDB_PORT_CONFIG_DIR ${CMAKE_SOURCE_DIR}/include/port)
configure_file(
  ${CMAKE_SOURCE_DIR}/include/port/port_config.h.in
  ${CMAKE_SOURCE_DIR}/include/port/port_config.h
)

add_executable(simple_leveldb main.cc 
  ${LEVELDB_HEADERS} 
  ${LEVELDB_SOURCES} 
  ${PORT_HEADERS}
  ${PORT_SOURCES}
  ${TABLE_HEADERS}
  ${TABLE_SOURCES}
  ${UTIL_HEADERS}
  ${UTIL_SOURCES}
  ${CMAKE_SOURCE_DIR}/include/port/port_config.h
)

target_compile_definitions(simple_leveldb
  PRIVATE
  ${SIMPLE_LEVELDB_PLATFORM_NAME}=1
)
