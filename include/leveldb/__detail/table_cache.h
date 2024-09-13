#ifndef STORAGE_SIMPLE_LEVELDB_DB_TABLE_CACHE_H
#define STORAGE_SIMPLE_LEVELDB_DB_TABLE_CACHE_H

#include "leveldb/cache.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include <cstdint>

namespace simple_leveldb {

	class table_cache {
	private:
		env* const         env_;
		const core::string dbname_;
		const options&     options_;
		cache*             cache_;

	public:
		table_cache( const core::string& dbname, const options& options, int32_t entries );
		table_cache( const table_cache& )            = delete;
		table_cache& operator=( const table_cache& ) = delete;
		~table_cache();

	public:
		void evict( uint64_t file_number );
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_DB_TABLE_CACHE_H
