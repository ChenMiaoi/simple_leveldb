cmake_minimum_required(VERSION 3.8.0)
project(simple_leveldb VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true) 

file(GLOB_RECURSE  LEVELDB_HEADERS ${CMAKE_SOURCE_DIR}/include/leveldb/*.h)
file(GLOB PORT_HEADERS ${CMAKE_SOURCE_DIR}/include/port/*.h)
file(GLOB TABLE_HEADERS ${CMAKE_SOURCE_DIR}/include/table/*.h)
file(GLOB UTIL_HEADERS ${CMAKE_SOURCE_DIR}/include/util/*.h)
file(GLOB_RECURSE  LEVELDB_SOURCES ${CMAKE_SOURCE_DIR}/include/leveldb/*.cc)
file(GLOB PORT_SOURCES ${CMAKE_SOURCE_DIR}/include/port/*.cc)
file(GLOB TABLE_SOURCES ${CMAKE_SOURCE_DIR}/include/table/*.cc)
file(GLOB UTIL_SOURCES ${CMAKE_SOURCE_DIR}/include/util/*.cc)
include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(simple_leveldb main.cc 
  ${LEVELDB_HEADERS} 
  ${PORT_HEADERS}
  ${TABLE_HEADERS}
  ${UTIL_HEADERS}
  ${LEVELDB_SOURCES} 
  ${PORT_SOURCES}
  ${TABLE_SOURCES}
  ${UTIL_SOURCES}
)

