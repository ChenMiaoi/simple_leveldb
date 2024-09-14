#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H

#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port_stdcxx.h"
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace simple_leveldb {

	namespace log {
		class writer;
	}

	class version_set;
	class table_cache;
	class compaction;
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
		struct level_summary_storage {
			char buffer[ 100 ];
		};
		const char* level_summary( level_summary_storage* scratch ) const;

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

		void        set_last_sequence( uint64_t );
		void        add_live_files( core::set< uint64_t >* live );
		void        mark_file_number_used( uint64_t number );
		bool        needs_compaction() const;
		compaction* pick_compaction();
		compaction* compact_range( int32_t level, const internal_key* begin, const internal_key* end );

	private:
		class builder;

		bool   reuse_manifest( const core::string& dscname, const core::string& dscbase );
		void   finalize( version* v );
		status write_snap_shot( log::writer* log );
		void   append_version( version* v );
	};

	class compaction {
		friend class verison;
		friend class version_set;

	private:
		int32_t      level_;
		uint64_t     max_output_file_size_;
		version*     input_version_;
		version_edit edit_;

		core::vector< file_meta_data* > input_[ 2 ];
		core::vector< file_meta_data* > grandparents_;
		size_t                          grandparent_index_;
		bool                            seen_key_;
		int64_t                         overlapped_bytes_;
		size_t                          level_ptrs[ config::kNumLevels ];

	private:
		compaction( const options* options, int32_t level );

	public:
		~compaction();

	public:
		int32_t         level() const;
		version_edit*   edit();
		int32_t         num_input_files( int32_t which ) const;
		file_meta_data* input( int32_t which, int32_t i ) const;
		uint64_t        max_output_file_size() const;
		bool            is_trivial_move() const;
		void            add_input_deletions( version_edit* edit );
		bool            is_base_level_for_key( const slice& user_key );
		bool            should_stop_before( const slice& internal_key );
		void            release_inputs();
	};
}// namespace simple_leveldb

#endif//!STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_VERSION_SET_H
