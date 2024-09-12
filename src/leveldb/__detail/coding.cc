#include "leveldb/__detail/coding.h"
#include <cstdint>

namespace simple_leveldb {

	void put_fixed32( core::string* dst, uint32_t value ) {
		char buf[ sizeof value ];
		encode_fixed32( buf, value );
		dst->append( buf, sizeof buf );
	}

	void put_fixed64( core::string* dst, uint64_t value ) {
		char buf[ sizeof value ];
		encode_fixed64( buf, value );
		dst->append( buf, sizeof buf );
	}

	void put_varint32( core::string* dst, uint32_t value ) {
		char  buf[ 5 ];
		char* ptr = encode_varint32( buf, value );
		dst->append( buf, ptr - buf );
	}

	inline void encode_fixed32( char* dst, uint32_t value ) {
		uint8_t* const buffer = reinterpret_cast< uint8_t* >( dst );

		buffer[ 0 ] = static_cast< uint8_t >( value );
		buffer[ 1 ] = static_cast< uint8_t >( value >> 8 );
		buffer[ 2 ] = static_cast< uint8_t >( value >> 16 );
		buffer[ 3 ] = static_cast< uint8_t >( value >> 24 );
	}

	inline void encode_fixed64( char* dst, uint64_t value ) {
		uint8_t* const buffer = reinterpret_cast< uint8_t* >( dst );

		// Recent clang and gcc optimize this to a single mov / str instruction.
		buffer[ 0 ] = static_cast< uint8_t >( value );
		buffer[ 1 ] = static_cast< uint8_t >( value >> 8 );
		buffer[ 2 ] = static_cast< uint8_t >( value >> 16 );
		buffer[ 3 ] = static_cast< uint8_t >( value >> 24 );
		buffer[ 4 ] = static_cast< uint8_t >( value >> 32 );
		buffer[ 5 ] = static_cast< uint8_t >( value >> 40 );
		buffer[ 6 ] = static_cast< uint8_t >( value >> 48 );
		buffer[ 7 ] = static_cast< uint8_t >( value >> 56 );
	}

	char* encode_varint32( char* dst, uint32_t value ) {
		uint8_t*             ptr = reinterpret_cast< uint8_t* >( dst );
		static const int32_t B   = 128;
		if ( value < ( 1 << 7 ) ) {
			*( ptr++ ) = value;
		} else if ( value < ( 1 << 14 ) ) {
			*( ptr++ ) = value | B;
			*( ptr++ ) = value >> 7;
		} else if ( value < ( 1 << 21 ) ) {
			*( ptr++ ) = value | B;
			*( ptr++ ) = ( value >> 7 ) | B;
			*( ptr++ ) = value >> 14;
		} else if ( value < ( 1 << 28 ) ) {
			*( ptr++ ) = value | B;
			*( ptr++ ) = ( value >> 7 ) | B;
			*( ptr++ ) = ( value >> 14 ) | B;
			*( ptr++ ) = value >> 21;
		} else {
			*( ptr++ ) = value | B;
			*( ptr++ ) = ( value >> 7 ) | B;
			*( ptr++ ) = ( value >> 14 ) | B;
			*( ptr++ ) = ( value >> 21 ) | B;
			*( ptr++ ) = value >> 28;
		}
		return reinterpret_cast< char* >( ptr );
	}

	void put_length_prefixd_slice( core::string* dst, const slice& value ) {
		put_varint32( dst, value.size() );
		dst->append( value.data(), value.size() );
	}

}// namespace simple_leveldb
