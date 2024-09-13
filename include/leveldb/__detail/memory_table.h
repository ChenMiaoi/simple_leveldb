#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_MEMORY_TABLE_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_MEMORY_TABLE_H

#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/skip_list.h"
#include "util/arena.h"
#include <cstdint>
namespace simple_leveldb {

	class mem_table {
		friend class mem_table_iterator;
		friend class mem_table_backward_iterator;

	private:
		struct key_comparator {
			const internal_key_comparator comparator;

			explicit key_comparator( const internal_key_comparator& c )
					: comparator( c ) {}

			int32_t operator()( const char* a, const char* b ) const;
		};

		using table = skip_list< const char*, key_comparator >;

	private:
		key_comparator comparator_;
		int32_t        refs_;
		arena          arena_;
		table          table_;

	public:
		explicit mem_table( const internal_key_comparator& comparator );
		mem_table( const mem_table& )            = delete;
		mem_table& operator=( const mem_table& ) = delete;
		~mem_table();

	public:
		void ref() { ++refs_; }
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_MEMORY_TABLE_H
