#include "leveldb/__detail/db_format.h"
#include "leveldb/slice.h"
#include "util/coding.h"
#include <cassert>
#include <cstdint>
#include <string>

namespace simple_leveldb {

	static uint64_t pack_sequence_and_type( uint64_t seq, value_type t ) {
		assert( seq <= kMaxSequenceNumber );
		assert( t <= kValueTypeForSeek );
		return ( seq << 8 ) | static_cast< int8_t >( t );
	}

	const char* internal_key_comparator::name() const {
		return "simple_leveldb.InternalKeyComparator";
	}

	int32_t internal_key_comparator::compare( const slice& akey, const slice& bkey ) const {
		int32_t r = user_comparator_->compare(
			extract_user_key( akey ), extract_user_key( bkey ) );
		if ( r == 0 ) {
			const uint64_t anum = decode_fixed64( akey.data() + akey.size() - 8 );
			const uint64_t bnum = decode_fixed64( bkey.data() + bkey.size() - 8 );
			if ( anum > bnum ) {
				return -1;
			} else if ( anum < bnum ) {
				return +1;
			}
		}
		return r;
	}

	int32_t internal_key_comparator::compare( const internal_key& a, const internal_key& b ) const {
		return compare( a.encode(), b.encode() );
	}

	void internal_key_comparator::find_shortest_separator( core::string* start, const slice& limit ) const {
		slice        user_start = extract_user_key( *start );
		slice        user_limit = extract_user_key( limit );
		core::string tmp( user_start.data(), user_start.size() );
		user_comparator_->find_shortest_separator( &tmp, user_limit );
		if ( tmp.size() < user_start.size() && user_comparator_->compare( user_start, tmp ) < 0 ) {
			put_fixed64( &tmp, pack_sequence_and_type( kMaxSequenceNumber, kValueTypeForSeek ) );
			assert( this->compare( *start, tmp ) < 0 );
			assert( this->compare( tmp, limit ) < 0 );
			start->swap( tmp );
		}
	}

	void internal_key_comparator::find_short_successor( core::string* key ) const {
		slice        user_key = extract_user_key( *key );
		core::string tmp( user_key.data(), user_key.size() );
		user_comparator_->find_short_successor( &tmp );
		if ( tmp.size() < user_key.size() && user_comparator_->compare( user_key, tmp ) < 0 ) {
			put_fixed64( &tmp, pack_sequence_and_type( kMaxSequenceNumber, kValueTypeForSeek ) );
			assert( this->compare( *key, tmp ) < 0 );
			key->swap( tmp );
		}
	}

	slice internal_key::encode() const {
		assert( !rep_.empty() );
		return rep_;
	}

}// namespace simple_leveldb
