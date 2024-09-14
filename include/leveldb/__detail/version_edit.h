#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_EDIT_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_EDIT_H

#include "leveldb/__detail/db_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstdint>
#include <set>
#include <string>
#include <sys/types.h>
#include <utility>
#include <vector>
namespace simple_leveldb {

	class version_set;

	struct file_meta_data {
		int32_t      refs;
		int32_t      allowed_seeks;
		uint64_t     number;
		uint64_t     file_size;
		internal_key smallest;
		internal_key largest;

		file_meta_data()
				: refs( 0 )
				, allowed_seeks( 1 << 30 )
				, file_size( 0 ) {}
	};

	class version_edit {
	private:
		friend class version_set;

		using deleted_file_set = core::set< core::pair< int32_t, uint64_t > >;

		core::string    comparator_;
		uint64_t        log_number_;
		uint64_t        prev_log_number_;
		uint64_t        next_file_number_;
		sequence_number last_sequence_;
		bool            has_comparator_;
		bool            has_log_number_;
		bool            has_prev_log_number_;
		bool            has_next_file_number_;
		bool            has_last_sequence_;

		core::vector< std::pair< int, internal_key > >   compact_pointers_;
		deleted_file_set                                 deleted_files_;
		core::vector< std::pair< int, file_meta_data > > new_files_;

	public:
		version_edit() { clear(); }
		~version_edit() = default;

	public:
		void clear();
		void set_comparator_name( const slice& name );
		void set_log_number( uint64_t num );
		void set_prev_log_number( uint64_t num );
		void set_next_file( uint64_t num );
		void set_last_sequence( sequence_number seq );
		void set_compact_pointer( int32_t level, const internal_key& key );

		void   encode_to( core::string* dst ) const;
		status decode_from( const slice& src );

		void add_file( int32_t level, uint64_t file, uint64_t file_size,
									 const internal_key& smallest, const internal_key& largest );
		void remove_file( int32_t level, uint64_t file );
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_EDIT_H
