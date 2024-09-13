#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_IMPL_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_IMPL_H

#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/filename.h"
#include "leveldb/__detail/memory_table.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/__detail/version_set.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"
#include "port/port_stdcxx.h"
#include <cstdint>
#include <set>

namespace simple_leveldb {

	class mem_table;
	class table_cache;
	class version;
	class version_edit;

	class db_impl : public db {
		friend class db;

	private:
		env*                          env_;
		const internal_key_comparator internal_comparator_;
		const internal_filter_policy  internal_filter_policy_;
		const options                 options_;
		const bool                    owns_info_log_;
		const core::string&           dbname_;

		table_cache* const table_cache_;

		file_lock* db_lock_;

		port::mutex    mtx_;
		mem_table*     mem_;
		writable_file* log_file_;
		uint64_t       logfile_number_;
		log::writer*   log_;

		core::set< uint64_t > pending_outputs_;

		version_set* const versions_;

		status bg_error_;

	public:
		db_impl( const options& option, const core::string& dbname );

		db_impl( const db_impl& )            = delete;
		db_impl& operator=( const db_impl& ) = delete;

		~db_impl() override;

	public:
		status Put( const write_options&, const slice& key, const slice& value ) override;
		status Write( const write_options&, write_batch* batch ) override;

	private:
		const comparator* user_comparator() const;

		status NewDB();
		status Recover( version_edit* edit, bool* save_manifest );
		void   RemoveObsoleteFiles();
		status RecoverLogFile( uint64_t log_number, bool last_log, bool* save_manifest,
													 version_edit* edit, sequence_number* max_sequence );
		status MaybeScheduleCompaction();
	};

	options sanitize_options( const core::string& db, const internal_key_comparator* icmp, const internal_filter_policy* i_policy, const options& src );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_IMPL_H
