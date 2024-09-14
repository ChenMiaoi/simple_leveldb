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
	void put_length_prefixed_slice( core::string* dst, const slice& value );

	bool        get_varint32( slice* input, uint32_t* value );
	bool        get_varint64( slice* input, uint64_t* value );
	const char* get_varint32ptr( const char* p, const char* limit, uint32_t* v );
	const char* get_varint64ptr( const char* p, const char* limit, uint64_t* v );
	const char* get_varint32ptr_fallback( const char* p, const char* limit, uint32_t* value );
	bool        get_length_prefixed_slice( slice* input, slice* result );

	void  encode_fixed32( char* dst, uint32_t value );
	void  encode_fixed64( char* dst, uint64_t value );
	char* encode_varint32( char* dst, uint32_t value );
	char* encode_varint64( char* dst, uint64_t value );

	static inline uint32_t decode_fixed32( const char* ptr );
	static inline uint64_t decode_fixed64( const char* ptr );

	static inline uint32_t decode_fixed32( const char* ptr ) {
		const uint8_t* const buffer = reinterpret_cast< const uint8_t* >( ptr );

		return ( static_cast< uint32_t >( buffer[ 0 ] ) ) |
					 ( static_cast< uint32_t >( buffer[ 1 ] ) << 8 ) |
					 ( static_cast< uint32_t >( buffer[ 2 ] ) << 16 ) |
					 ( static_cast< uint32_t >( buffer[ 3 ] ) << 24 );
	}

	static inline uint64_t decode_fixed64( const char* ptr ) {
		const uint8_t* const buffer = reinterpret_cast< const uint8_t* >( ptr );

		// Recent clang and gcc optimize this to a single mov / ldr instruction.
		return ( static_cast< uint64_t >( buffer[ 0 ] ) ) |
					 ( static_cast< uint64_t >( buffer[ 1 ] ) << 8 ) |
					 ( static_cast< uint64_t >( buffer[ 2 ] ) << 16 ) |
					 ( static_cast< uint64_t >( buffer[ 3 ] ) << 24 ) |
					 ( static_cast< uint64_t >( buffer[ 4 ] ) << 32 ) |
					 ( static_cast< uint64_t >( buffer[ 5 ] ) << 40 ) |
					 ( static_cast< uint64_t >( buffer[ 6 ] ) << 48 ) |
					 ( static_cast< uint64_t >( buffer[ 7 ] ) << 56 );
	}

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_CODING_H
