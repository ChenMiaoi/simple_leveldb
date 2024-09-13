#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H

#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include "port/port_stdcxx.h"
#include <cstdint>
#include <set>
#include <vector>

namespace simple_leveldb {

	namespace log {
		class writer;
	}

	class version_set;
	class table_cache;
	class internal_key_comparator;

	class version {
		friend class compaction;
		friend class version_set;

	private:
		version_set*                    vset_;
		version*                        next_;
		version*                        prev_;
		int32_t                         refs_;
		core::vector< file_meta_data* > files_[ config::kNumLevels ];
		file_meta_data*                 file_to_compact_;
		int32_t                         file_to_compact_level_;
		double                          compaction_score_;
		int32_t                         compaction_level_;

	public:
		explicit version( version_set* vset );
		version( const version& )            = delete;
		version& operator=( const version& ) = delete;
		~version();

	public:
		void ref();
		void un_ref();
	};

	class version_set {
		friend class compaction;
		friend class version;

	private:
		env* const                    env_;
		const core::string            dbname_;
		const options* const          options_;
		table_cache* const            table_cache_;
		const internal_key_comparator icmp_;
		uint64_t                      next_file_number_;
		uint64_t                      manifest_file_number_;
		uint64_t                      last_sequence_;
		uint64_t                      log_number_;
		uint64_t                      prev_log_number_;// 0 or backing store for memtable being compacted

		// Opened lazily
		writable_file* descriptor_file_;
		log::writer*   descriptor_log_;
		version        dummy_versions_;// Head of circular doubly-linked list of versions.
		version*       current_;       // == dummy_versions_.prev_

		// Per-level key at which the next compaction at that level should start.
		// Either an empty string, or a valid InternalKey.
		std::string compact_pointer_[ config::kNumLevels ];

	public:
		version_set( const core::string& dbname, const options* option,
								 table_cache* table_cache, const internal_key_comparator* );
		version_set( const version_set& )            = delete;
		version_set& operator=( const version_set& ) = delete;
		~version_set();

	public:
		status   log_any_apply( version_edit* edit, port::mutex* mtx );
		status   recover( bool* save_manifest );
		uint64_t new_file_number() { return next_file_number_++; }
		uint64_t log_number() { return log_number_; }
		uint64_t prev_log_number() { return prev_log_number_; }
		uint64_t last_sequence() const { return last_sequence_; }
		uint64_t manifest_file_number() const { return manifest_file_number_; }

		void set_last_sequence( uint64_t );

		void add_live_files( core::set< uint64_t >* live );
		void mark_file_number_used( uint64_t number );

	private:
		class builder;

		void   finalize( version* v );
		status write_snap_shot( log::writer* log );
		void   append_version( version* v );
	};
}// namespace simple_leveldb

#endif//!STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H
