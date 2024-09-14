#ifndef STORAGE_SIMPLE_LEVELDB_UTIL_LOGGING_H
#define STORAGE_SIMPLE_LEVELDB_UTIL_LOGGING_H

#include "leveldb/slice.h"
#include <cstdint>
#include <string>
namespace simple_leveldb {

	void append_number_to( core::string* str, uint64_t num );

	void append_escape_string_to( core::string* str, const slice& value );

	core::string number_to_string( uint64_t num );

	core::string escape_string( const slice& value );

	bool consume_decimal_number( slice* in, uint64_t* val );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_UTIL_LOGGING_H
