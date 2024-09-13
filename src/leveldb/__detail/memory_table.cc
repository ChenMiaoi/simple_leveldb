#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/memory_table.h"
#include <cassert>

namespace simple_leveldb {

	mem_table::mem_table( const internal_key_comparator& comparator )
			: comparator_( comparator )
			, refs_( 0 )
			, table_( comparator_, &arena_ ) {}

	mem_table::~mem_table() {
		assert( refs_ == 0 );
	}

}// namespace simple_leveldb
