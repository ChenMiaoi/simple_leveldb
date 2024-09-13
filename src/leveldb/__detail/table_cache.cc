#include "leveldb/__detail/table_cache.h"
#include "leveldb/slice.h"
#include "util/coding.h"
#include <cstdint>

namespace simple_leveldb {

	void table_cache::evict( uint64_t file_number ) {
		char buf[ sizeof file_number ];
		encode_fixed64( buf, file_number );
		cache_->erase( slice( buf, sizeof buf ) );
	}

}// namespace simple_leveldb
