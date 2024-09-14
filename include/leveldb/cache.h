#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_CACHE_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_CACHE_H

#include "leveldb/slice.h"
#include <cstddef>
#include <cstdint>
#include <functional>

namespace simple_leveldb {

	class cache;

	cache* new_lru_cache( size_t capacity );

	class cache {
	public:
		cache()                          = default;
		cache( const cache& )            = delete;
		cache& operator=( const cache& ) = delete;
		virtual ~cache();

	public:
		struct handle {};
		virtual handle*  insert( const slice& key, void* value, size_t charge,
														 core::function< void( const slice& key, void* value ) > deleter ) = 0;
		virtual handle*  look_up( const slice& key )                                               = 0;
		virtual void     release( handle* handle )                                                 = 0;
		virtual void*    value( handle* handle )                                                   = 0;
		virtual void     erase( const slice& key )                                                 = 0;
		virtual uint64_t new_id()                                                                  = 0;
		virtual void     prune() {}
		virtual size_t   total_charge() const = 0;
	};
}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_CACHE_H
