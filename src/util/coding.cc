#include "leveldb/slice.h"
#include "util/coding.h"
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

	void put_varint64( core::string* dst, uint64_t value ) {
		char  buf[ 10 ];
		char* ptr = encode_varint64( buf, value );
		dst->append( buf, ptr - buf );
	}

	bool get_varint32( slice* input, uint32_t* value ) {
		const char* p     = input->data();
		const char* limit = p + input->size();
		const char* q     = get_varint32ptr( p, limit, value );
		if ( q == nullptr ) {
			return false;
		} else {
			*input = slice( q, limit - q );
			return true;
		}
	}

	bool get_varint64( slice* input, uint64_t* value ) {
		const char* p     = input->data();
		const char* limit = p + input->size();
		const char* q     = get_varint64ptr( p, limit, value );
		if ( q == nullptr ) {
			return false;
		} else {
			*input = slice( q, limit - q );
			return true;
		}
	}

	inline const char* get_varint32ptr( const char* p, const char* limit, uint32_t* value ) {
		if ( p < limit ) {
			uint32_t result = *( reinterpret_cast< const uint8_t* >( p ) );
			if ( ( result & 128 ) == 0 ) {
				*value = result;
				return p + 1;
			}
		}
		return get_varint32ptr_fallback( p, limit, value );
	}

	const char* get_varint64ptr( const char* p, const char* limit, uint64_t* value ) {
		uint64_t result = 0;
		for ( uint32_t shift = 0; shift <= 63 && p < limit; shift += 7 ) {
			uint64_t byte = *( reinterpret_cast< const uint8_t* >( p ) );
			p++;
			if ( byte & 128 ) {
				result |= ( ( byte & 127 ) << shift );
			} else {
				result |= ( byte << shift );
				*value = result;
				return reinterpret_cast< const char* >( p );
			}
		}
		return nullptr;
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

	char* encode_varint64( char* dst, uint64_t v ) {
		static const int B   = 128;
		uint8_t*         ptr = reinterpret_cast< uint8_t* >( dst );
		while ( v >= B ) {
			*( ptr++ ) = v | B;
			v >>= 7;
		}
		*( ptr++ ) = static_cast< uint8_t >( v );
		return reinterpret_cast< char* >( ptr );
	}

	void put_length_prefixed_slice( core::string* dst, const slice& value ) {
		put_varint32( dst, value.size() );
		dst->append( value.data(), value.size() );
	}

	bool get_length_prefixed_slice( slice* input, slice* result ) {
		if ( uint32_t len; get_varint32( input, &len ) && input->size() >= len ) {
			*result = slice( input->data(), len );
			input->remove_prefix( len );
			return true;
		} else {
			return false;
		}
	}

	const char* get_varint32ptr_fallback( const char* p, const char* limit, uint32_t* value ) {
		uint32_t result = 0;
		for ( uint32_t shift = 0; shift <= 28 && p < limit; shift += 7 ) {
			uint32_t byte = *( reinterpret_cast< const uint8_t* >( p ) );
			p++;
			if ( byte & 128 ) {
				result |= ( ( byte & 127 ) << shift );
			} else {
				result |= ( byte << shift );
				*value = result;
				return reinterpret_cast< const char* >( p );
			}
		}
		return nullptr;
	}

}// namespace simple_leveldb
