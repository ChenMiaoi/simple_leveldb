#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/db_impl.h"
#include "leveldb/__detail/filename.h"
#include "leveldb/__detail/log_write.h"
#include "leveldb/__detail/memory_table.h"
#include "leveldb/__detail/table_cache.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/__detail/version_set.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace simple_leveldb {

	db::~db() = default;

	status db::open( const options& options, const core::string& name, db** dbptr ) {
		*dbptr = nullptr;

		db_impl* impl = new db_impl( options, name );
		impl->mtx_.lock();
		version_edit edit;
		bool         save_manifest = false;
		status       s             = impl->Recover( &edit, &save_manifest );
		if ( s.is_ok() && impl->mem_ == nullptr ) {
			uint64_t       new_logger_number = impl->versions_->new_file_number();
			writable_file* file;
			s = options.env->new_writable_file(
				log_file_name( name, new_logger_number ), &file );

			if ( s.is_ok() ) {
				edit.set_log_number( new_logger_number );
				impl->log_file_       = file;
				impl->logfile_number_ = new_logger_number;
				impl->log_            = new log::writer( file );
				impl->mem_            = new mem_table( impl->internal_comparator_ );
				impl->mem_->ref();
			}
		}

		if ( s.is_ok() && save_manifest ) {
			edit.set_prev_log_number( 0 );
			edit.set_log_number( impl->logfile_number_ );
			s = impl->versions_->log_any_apply( &edit, &impl->mtx_ );
		}
		if ( s.is_ok() ) {
			impl->RemoveObsoleteFiles();
			impl->MaybeScheduleCompaction();
		}
		impl->mtx_.unlock();
		if ( s.is_ok() ) {
			assert( impl->mem_ != nullptr );
			*dbptr = impl;
		} else {
			delete impl;
		}
		return s;
	}

	status db::Put( const write_options& opt, const slice& key, const slice& value ) {
		write_batch batch;
		batch.Put( key, value );
		return Write( opt, &batch );
	}

	const int kNumNonTableCacheFiles = 10;

	static int32_t table_cache_size( const options& sanitized_options ) {
		return sanitized_options.max_open_files - kNumNonTableCacheFiles;
	}

	db_impl::db_impl( const options& raw_option, const core::string& dbname )
			: env_( raw_option.env )
			, internal_comparator_( raw_option.comparator )
			, internal_filter_policy_( raw_option.filter_policy )
			, options_( sanitize_options( dbname, &internal_comparator_, &internal_filter_policy_, raw_option ) )
			, owns_info_log_( raw_option.info_log )
			, dbname_( dbname )
			, table_cache_( new table_cache( dbname_, options_, table_cache_size( options_ ) ) )
			, db_lock_( nullptr )
			, mem_( nullptr )
			, log_file_( nullptr )
			, logfile_number_( 0 )
			, log_( nullptr )
			, versions_( new version_set( dbname_, &options_, table_cache_, &internal_comparator_ ) ) {}

	db_impl::~db_impl() {}

	const comparator* db_impl::user_comparator() const {
		return internal_comparator_.user_comparator();
	}

	status db_impl::NewDB() {
		version_edit new_db;
		new_db.set_comparator_name( user_comparator()->name() );
		new_db.set_log_number( 0 );
		new_db.set_next_file( 2 );
		new_db.set_last_sequence( 0 );

		const core::string manifest = descriptor_file_name( dbname_, 1 );
		writable_file*     file;
		status             s = env_->new_writable_file( manifest, &file );
		if ( !s.is_ok() ) {
			return s;
		}
		{
			log::writer  log( file );
			core::string record;
			new_db.encode_to( &record );
			s = log.add_record( record );
			if ( s.is_ok() ) {
				s = file->sync();
			}
			if ( s.is_ok() ) {
				s = file->close();
			}
		}
		delete file;
		if ( s.is_ok() ) {
			s = set_current_file( env_, dbname_, 1 );
		} else {
			env_->remove_file( manifest );
		}
		return s;
	}

	void db_impl::RemoveObsoleteFiles() {
		mtx_.assert_held();
		if ( !bg_error_.is_ok() ) {
			return;
		}

		core::set< uint64_t > live = pending_outputs_;
		versions_->add_live_files( &live );

		core::vector< core::string > filenames;
		env_->get_children( dbname_, &filenames );
		uint64_t                     number;
		file_type                    type;
		core::vector< core::string > files_to_delete;
		for ( auto& filename: filenames ) {
			if ( parse_file_name( filename, &number, &type ) ) {
				bool keep = true;
				switch ( type ) {
					case file_type::kLogFile:
						keep = ( ( number >= versions_->log_number() ) ||
										 ( number == versions_->prev_log_number() ) );
						break;
					case file_type::kTableFile:
						keep = ( live.find( number ) != live.end() );
						break;
					case file_type::kDescriptorFile:
						keep = ( number >= versions_->manifest_file_number() );
						break;
					case file_type::kTempFile:
						keep = ( live.find( number ) != live.end() );
						break;
					case file_type::kCurrentFile:
					case file_type::kDBLockFile:
					case file_type::kInfoLogFile:
						keep = true;
						break;
				}
				if ( !keep ) {
					files_to_delete.emplace_back( core::move( filename ) );
					if ( type == file_type::kTableFile ) {
						table_cache_->evict( number );
					}
					Log( options_.info_log, "Delete type=%d #%lld\n",
							 static_cast< int32_t >( type ), static_cast< unsigned long long >( number ) );
				}
			}
		}
		mtx_.unlock();
		for ( const auto& filename: files_to_delete ) {
			env_->remove_file( dbname_ + "/" + filename );
		}
		mtx_.lock();
	}

	status db_impl::Recover( version_edit* edit, bool* save_manifest ) {
		mtx_.assert_held();

		env_->create_dir( dbname_ );
		assert( db_lock_ == nullptr );
		status s = env_->lock_file( lock_file_name( dbname_ ), &db_lock_ );
		if ( !s.is_ok() ) {
			return s;
		}

		if ( !env_->file_exists( current_file_name( dbname_ ) ) ) {
			if ( options_.create_if_missing ) {
				Log( options_.info_log, "Creating DB %s since it was missing.", dbname_.c_str() );
				s = NewDB();
				if ( !s.is_ok() ) {
					return s;
				}
			} else {
				return status::invalid_argument( dbname_, "does not exist (create_if_missing is false)" );
			}
		} else {
			if ( options_.error_if_exists ) {
				return status::invalid_argument( dbname_, "exists (error_if_exists is true)" );
			}
		}

		s = versions_->recover( save_manifest );
		if ( !s.is_ok() ) {
			return s;
		}

		sequence_number max_sequence{ 0 };

		const uint64_t               min_log  = versions_->log_number();
		const uint64_t               prev_log = versions_->prev_log_number();
		core::vector< core::string > filenames;
		s = env_->get_children( dbname_, &filenames );
		if ( !s.is_ok() ) {
			return s;
		}

		core::set< uint64_t > expected;
		versions_->add_live_files( &expected );
		uint64_t                 number;
		file_type                f_type;
		core::vector< uint64_t > logs;

		for ( auto& filename: filenames ) {
			if ( parse_file_name( filename, &number, &f_type ) ) {
				expected.erase( number );
				if ( f_type == file_type::kLogFile && ( ( number >= min_log ) || ( number == prev_log ) ) ) {
					logs.emplace_back( number );
				}
			}
			if ( !expected.empty() ) {
				char buf[ 50 ];
				core::snprintf( buf, sizeof buf,
												"%d missing files; e.g.",
												static_cast< int32_t >( expected.size() ) );
				return status::corruption( buf, table_file_name( dbname_, *( expected.begin() ) ) );
			}
		}

		core::sort( logs.begin(), logs.end() );
		for ( int32_t i = 0; auto log: logs ) {
			s = RecoverLogFile( log, ( i++ == logs.size() - 1 ), save_manifest, edit, &max_sequence );
			if ( !s.is_ok() ) {
				return s;
			}
			versions_->mark_file_number_used( log );
		}

		if ( versions_->last_sequence() < max_sequence ) {
			versions_->set_last_sequence( max_sequence );
		}
		return status::ok();
	}

}// namespace simple_leveldb
