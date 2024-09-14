#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "util/coding.h"
#include <cstdint>
#include <string>
#include <utility>

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

	void version_edit::set_comparator_name( const slice& name ) {
		has_comparator_ = true;
		comparator_     = name.to_string();
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

	void version_edit::set_compact_pointer( int32_t level, const internal_key& key ) {
		compact_pointers_.emplace_back( core::make_pair( level, key ) );
	}

	void version_edit::encode_to( core::string* dst ) const {
		if ( has_comparator_ ) {
			put_varint32( dst, kComparator );
			put_length_prefixed_slice( dst, comparator_ );
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
			put_length_prefixed_slice( dst, key.encode().to_string() );
		}

		for ( auto [ level, f ]: new_files_ ) {
			const file_meta_data& file = f;
			put_varint32( dst, kNewFile );
			put_varint32( dst, level );
			put_varint64( dst, file.number );
			put_varint64( dst, file.file_size );
			put_length_prefixed_slice( dst, file.smallest.encode() );
			put_length_prefixed_slice( dst, file.largest.encode() );
		}
	}

	static bool get_internal_key( slice* input, internal_key* dst ) {
		if ( slice str; get_length_prefixed_slice( input, &str ) ) {
			return dst->decode_from( str );
		}
		return false;
	}

	static bool get_level( slice* input, int32_t* level ) {
		if ( uint32_t v; get_varint32( input, &v ) && v < config::kNumLevels ) {
			*level = v;
			return true;
		}
		return false;
	}

	status version_edit::decode_from( const slice& src ) {
		clear();
		slice       input = src;
		const char* msg   = nullptr;
		uint32_t    tag;

		int32_t        level;
		uint64_t       number;
		file_meta_data f;
		slice          str;
		internal_key   key;

		while ( msg == nullptr && get_varint32( &input, &tag ) ) {
			switch ( tag ) {
				case kComparator:
					if ( get_length_prefixed_slice( &input, &str ) ) {
						comparator_     = str.to_string();
						has_comparator_ = true;
					} else {
						msg = "comparator name";
					}
					break;
				case kLogNumber:
					if ( get_varint64( &input, &log_number_ ) ) {
						has_log_number_ = true;
					} else {
						msg = "log number";
					}
					break;
				case kPrevLogNumber:
					if ( get_varint64( &input, &prev_log_number_ ) ) {
						has_prev_log_number_ = true;
					} else {
						msg = "previous log number";
					}
					break;
				case kNextFileNumber:
					if ( get_varint64( &input, &next_file_number_ ) ) {
						has_next_file_number_ = true;
					} else {
						msg = "next file number";
					}
					break;
				case kLastSequence:
					if ( get_varint64( &input, &last_sequence_ ) ) {
						has_last_sequence_ = true;
					} else {
						msg = "last sequence number";
					}
					break;
				case kCompactPointer:
					if ( get_level( &input, &level ) && get_varint64( &input, &number ) ) {
						deleted_files_.insert( { level, number } );
					} else {
						msg = "deleted file";
					}
					break;
				case kNewFile:
					if ( get_level( &input, &level ) && get_varint64( &input, &f.number ) &&
							 get_varint64( &input, &f.file_size ) &&
							 get_internal_key( &input, &f.smallest ) &&
							 get_internal_key( &input, &f.largest ) ) {
						new_files_.emplace_back( core::make_pair( level, f ) );
					} else {
						msg = "new-file entry";
					}
					break;
				default:
					msg = "unknown tag";
					break;
			}
		}

		if ( msg == nullptr && !input.empty() ) {
			msg = "invalid tag";
		}
		status result;
		if ( msg != nullptr ) {
			result = status::corruption( "version_edit", msg );
		}
		return result;
	}

	void version_edit::add_file( int32_t level, uint64_t file, uint64_t file_size,
															 const internal_key& smallest, const internal_key& largest ) {
		file_meta_data f;
		f.number    = file;
		f.file_size = file_size;
		f.smallest  = smallest;
		f.largest   = largest;
		new_files_.emplace_back( core::make_pair( level, f ) );
	}

}// namespace simple_leveldb
