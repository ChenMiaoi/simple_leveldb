#include "leveldb/status.h"
#include <cassert>
#include <cstdint>
#include <cstring>

namespace simple_leveldb {

	const char* status::copy_state( const char* state ) {
		uint32_t size;
		::memcpy( &size, state, sizeof size );
		char* result = new char[ size + 5 ];
		::memcpy( result, state, size + 5 );
		return result;
	}

	status::status( enum code code, const slice& msg, const slice& msg2 ) {
		assert( code != code::kOk );

		const uint32_t len1 = static_cast< uint32_t >( msg.size() );
		const uint32_t len2 = static_cast< uint32_t >( msg2.size() );
		const int32_t  size = len1 + ( len2 ? ( 2 + len2 ) : 0 );

		char* result = new char[ size + 5 ];
		::memcpy( result, &size, sizeof size );
		result[ 4 ] = static_cast< char >( code );
		::memcpy( result + 5, msg.data(), len1 );
		if ( len2 ) {
			result[ 5 + len1 ] = ':';
			result[ 6 + len1 ] = ' ';
			::memcpy( result + 7 + len1, msg2.data(), len2 );
		}
		state_ = result;
	}

}// namespace simple_leveldb
