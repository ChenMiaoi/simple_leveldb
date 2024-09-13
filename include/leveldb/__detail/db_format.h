#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H

#include "leveldb/comparator.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include <cassert>
#include <cstdint>
#include <string>

namespace simple_leveldb {

	namespace core = std;

	namespace config {
		static const int kNumLevels            = 7;
		static const int kL0_CompactionTrigger = 4;
	}// namespace config

	class internal_key;

	// Value types encoded as the last component of internal keys.
	// DO NOT CHANGE THESE ENUM VALUES: they are embedded in the on-disk
	// data structures.
	enum class value_type : int8_t {
		kTypeDeletion = 0x00,
		kTypeValue    = 0x01,
	};

	// kValueTypeForSeek defines the ValueType that should be passed when
	// constructing a ParsedInternalKey object for seeking to a particular
	// sequence number (since we sort sequence numbers in decreasing order
	// and the value type is embedded as the low 8 bits in the sequence
	// number in internal keys, we need to use the highest-numbered
	// ValueType, not the lowest).
	static const value_type kValueTypeForSeek = value_type::kTypeValue;

	using sequence_number                           = uint64_t;
	static const sequence_number kMaxSequenceNumber = ( ( 0x1ull << 56 ) - 1 );

	struct parsed_internal_key {
		slice           user_key;
		sequence_number sequence;
		value_type      type;

		parsed_internal_key() = default;
		parsed_internal_key( const slice& u, const sequence_number& seq, value_type& t )
				: user_key( u )
				, sequence( seq )
				, type( t ) {}

		core::string debug_string() const;
	};

	void         append_internal_key( core::string* result, const parsed_internal_key& key );
	inline slice extract_user_key( const slice& internal_key ) {
		assert( internal_key.size() >= 8 );
		return slice( internal_key.data(), internal_key.size() - 8 );
	}

	class internal_key_comparator : public comparator {
	private:
		const comparator* user_comparator_;

	public:
		explicit internal_key_comparator( const comparator* c )
				: user_comparator_( c ) {}

		const char*       name() const override;
		int32_t           compare( const slice& a, const slice& b ) const override;
		int32_t           compare( const internal_key& a, const internal_key& b ) const;
		void              find_shortest_separator( core::string* start, const slice& limit ) const override;
		void              find_short_successor( core::string* key ) const override;
		const comparator* user_comparator() const { return user_comparator_; }
	};

	class internal_filter_policy : public filter_policy {
	private:
		const filter_policy* const user_policy_;

	public:
		explicit internal_filter_policy( const filter_policy* p )
				: user_policy_( p ) {}

	public:
		const char* name() const override;
		void        create_filter( const slice& keys, int32_t n, core::string& dst ) const override;
		bool        key_may_match( const slice& key, const slice& filter ) const override;
	};

	class internal_key {
	private:
		core::string rep_;

	public:
		internal_key() = default;
		internal_key( const slice& user_key, sequence_number s, value_type t ) {
			append_internal_key( &rep_, parsed_internal_key( user_key, s, t ) );
		}

	public:
		bool         decode_from( const slice& s );
		slice        encode() const;
		slice        user_key() const;
		void         set_from( const parsed_internal_key& p );
		void         clear();
		core::string debug_string() const;
	};


}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H
