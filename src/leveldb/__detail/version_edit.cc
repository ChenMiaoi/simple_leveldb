#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/version_edit.h"
#include "util/coding.h"
#include <cstdint>
#include <string>

namespace simple_leveldb {

	enum tag {
		kComparator     = 1,
		kLogNumber      = 2,
		kNextFileNumber = 3,
		kLastSequence   = 4,
		kCompactPointer = 5,
		kDeletedFile    = 6,
		kNewFile        = 7,
		// 8 was used for large value refs
		kPrevLogNumber = 9
	};

	void version_edit::clear() {
		comparator_.clear();
		log_number_           = 0;
		prev_log_number_      = 0;
		last_sequence_        = 0;
		next_file_number_     = 0;
		has_comparator_       = false;
		has_log_number_       = false;
		has_prev_log_number_  = false;
		has_next_file_number_ = false;
		has_last_sequence_    = false;
		compact_pointers_.clear();
		deleted_files_.clear();
		new_files_.clear();
	}

	void version_edit::set_log_number( uint64_t num ) {
		has_log_number_ = true;
		log_number_     = num;
	}

	void version_edit::set_prev_log_number( uint64_t num ) {
		has_prev_log_number_ = true;
		prev_log_number_     = num;
	}

	void version_edit::set_next_file( uint64_t num ) {
		has_next_file_number_ = true;
		next_file_number_     = num;
	}

	void version_edit::set_last_sequence( sequence_number seq ) {
		has_last_sequence_ = true;
		last_sequence_     = seq;
	}

	void version_edit::encode_to( core::string* dst ) const {
		if ( has_comparator_ ) {
			put_varint32( dst, kComparator );
			put_length_prefixd_slice( dst, comparator_ );
		}
		if ( has_log_number_ ) {
			put_varint32( dst, kLogNumber );
			put_varint64( dst, log_number_ );
		}
		if ( has_prev_log_number_ ) {
			put_varint32( dst, kPrevLogNumber );
			put_varint64( dst, prev_log_number_ );
		}
		if ( has_next_file_number_ ) {
			put_varint32( dst, kNextFileNumber );
			put_varint64( dst, next_file_number_ );
		}
		if ( has_last_sequence_ ) {
			put_varint32( dst, kLastSequence );
			put_varint64( dst, last_sequence_ );
		}

		for ( auto [ level, key ]: compact_pointers_ ) {
			put_varint32( dst, kCompactPointer );
			put_varint32( dst, level );
			put_length_prefixd_slice( dst, key.encode().to_string() );
		}

		for ( auto [ level, f ]: new_files_ ) {
			const file_meta_data& file = f;
			put_varint32( dst, kNewFile );
			put_varint32( dst, level );
			put_varint64( dst, file.number );
			put_varint64( dst, file.file_size );
			put_length_prefixd_slice( dst, file.smallest.encode() );
			put_length_prefixd_slice( dst, file.largest.encode() );
		}
	}

}// namespace simple_leveldb
