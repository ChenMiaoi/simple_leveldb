#include "leveldb/__detail/log_format.h"
#include "leveldb/__detail/log_write.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include <cassert>
#include <cstddef>
#include <cstdint>

namespace simple_leveldb::log {

	static void init_type_crc( uint32_t* type_crc ) {
		for ( int32_t i = 0; i < kMaxRecordType; i++ ) {
			char t        = static_cast< char >( i );
			type_crc[ i ] = crc32c::Value( &t, 1 );
		}
	}

	writer::writer( writable_file* dest )
			: dest_( dest )
			, block_offset_( 0 ) {
		init_type_crc( type_crc_ );
	}

	writer::writer( writable_file* dest, uint64_t dest_length )
			: dest_( dest )
			, block_offset_( dest_length % kBlockSize ) {
		init_type_crc( type_crc_ );
	}

	writer::~writer() = default;

	status writer::add_record( const slice& sle ) {
		const char* ptr  = sle.data();
		size_t      left = sle.size();

		status s;
		bool   begin = true;

		do {
			const int32_t left_over = kBlockSize - block_offset_;
			assert( left_over >= 0 );
			if ( left_over < kHeaderSize ) {
				if ( left_over > 0 ) {
					static_assert( kHeaderSize == 7, " " );
					dest_->append( slice( "\x00\x00\x00\x00\x00\x00", left_over ) );
				}
				block_offset_ = 0;
			}

			assert( kBlockSize - block_offset_ - kHeaderSize >= 0 );

			const size_t avail           = kBlockSize - block_offset_ - kHeaderSize;
			const size_t fragment_length = ( left < avail ) ? left : avail;

			record_type type;
			const bool  end = ( left == fragment_length );
			if ( begin && end ) {
				type = record_type::kFullType;
			} else if ( begin ) {
				type = record_type::kFirstType;
			} else if ( begin ) {
				type = record_type::kLastType;
			} else {
				type = record_type::kMiddleType;
			}

			s = emit_physical_record( type, ptr, fragment_length );
			ptr += fragment_length;
			left -= fragment_length;
			begin = false;
		} while ( s.is_ok() && left > 0 );

		return s;
	}

	status writer::emit_physical_record( record_type type, const char* ptr, size_t length ) {
		assert( length <= 0xffff );
		assert( block_offset_ + kHeaderSize + length <= kBlockSize );

		char buf[ kHeaderSize ];
		buf[ 4 ] = static_cast< char >( length & 0xff );
		buf[ 5 ] = static_cast< char >( length >> 8 );
		buf[ 6 ] = static_cast< char >( type );

		uint32_t crc = crc32c::Extend( type_crc_[ static_cast< int32_t >( type ) ], ptr, length );
		crc          = crc32c::Mask( crc );
		encode_fixed32( buf, crc );

		status s = dest_->append( slice( buf, kHeaderSize ) );
		if ( s.is_ok() ) {
			s = dest_->append( slice( ptr, length ) );
			if ( s.is_ok() ) {
				s = dest_->flush();
			}
		}
		block_offset_ += kHeaderSize + length;
		return s;
	}

}// namespace simple_leveldb::log
