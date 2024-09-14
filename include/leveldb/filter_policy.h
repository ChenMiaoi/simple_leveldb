#ifndef STORAGE_SIMPLE_LEVELDB_INCLUDE_FILTER_POLICY_H
#define STORAGE_SIMPLE_LEVELDB_INCLUDE_FILTER_POLICY_H

#include "leveldb/slice.h"
#include <cstdint>
#include <string>

namespace simple_leveldb {

	class filter_policy {
	public:
		virtual ~filter_policy();

	public:
		virtual const char* name() const                                                           = 0;
		virtual void        create_filter( const slice* keys, int32_t n, core::string& dst ) const = 0;
		virtual bool        key_may_match( const slice& key, const slice& filter ) const           = 0;
	};

	const filter_policy* new_bloom_filter_policy( int32_t bits_per_key );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_INCLUDE_FILTER_POLICY_H
