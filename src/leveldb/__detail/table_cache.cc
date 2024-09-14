#include "leveldb/__detail/table_cache.h"
#include "leveldb/cache.h"
#include "leveldb/slice.h"
#include "util/coding.h"
#include <cstdint>

namespace simple_leveldb {

	table_cache::table_cache( const core::string& dbname, const options& options, int32_t entries )
			: env_( options.env )
			, dbname_( dbname )
			, options_( options )
			, cache_( new_lru_cache( entries ) ) {}

	table_cache::~table_cache() { delete cache_; }

	void table_cache::evict( uint64_t file_number ) {
		char buf[ sizeof file_number ];
		encode_fixed64( buf, file_number );
		cache_->erase( slice( buf, sizeof buf ) );
	}

}// namespace simple_leveldb
