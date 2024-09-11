#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_CODING_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_CODING_H

#include "leveldb/slice.h"
#include <cstdint>
#include <string>

namespace simple_leveldb {

	void put_fixed32( core::string* dst, uint32_t value );
	void put_fixed64( core::string* dst, uint64_t value );
	void put_varint32( core::string* dst, uint32_t value );
	void put_varint64( core::string* dst, uint64_t value );
	void put_length_prefixd_slice( core::string* dst, const slice& value );

	void encode_fixed32( char* dst, uint32_t value );
	void encode_fixed64( char* dst, uint64_t value );

	uint32_t decode_fixed32( const char* ptr );
	uint64_t decode_fixed64( const char* ptr );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_CODING_H
