#include "leveldb/__detail/coding.h"
#include <cstdint>

namespace simple_leveldb {

	void put_fixed32( core::string* dst, uint32_t value ) {
		char buf[ sizeof value ];
		encode_fixed32( buf, value );
		dst->append( buf, sizeof buf );
	}

	inline void encode_fixed32( char* dst, uint32_t value ) {
		uint8_t* const buffer = reinterpret_cast< uint8_t* >( dst );

		buffer[ 0 ] = static_cast< uint8_t >( value );
		buffer[ 1 ] = static_cast< uint8_t >( value >> 8 );
		buffer[ 2 ] = static_cast< uint8_t >( value >> 16 );
		buffer[ 3 ] = static_cast< uint8_t >( value >> 24 );
	}

	inline uint32_t decode_fixed32( const char* ptr ) {
		const uint8_t* const buffer = reinterpret_cast< const uint8_t* >( ptr );

		return ( static_cast< uint32_t >( buffer[ 0 ] ) ) |
					 ( static_cast< uint32_t >( buffer[ 1 ] ) << 8 ) |
					 ( static_cast< uint32_t >( buffer[ 2 ] ) << 16 ) |
					 ( static_cast< uint32_t >( buffer[ 3 ] ) << 24 );
	}

}// namespace simple_leveldb
