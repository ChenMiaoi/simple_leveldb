#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/db_impl.h"
#include "leveldb/__detail/filename.h"
#include "leveldb/__detail/log_write.h"
#include "leveldb/__detail/memory_table.h"
#include "leveldb/__detail/table_cache.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/__detail/version_set.h"
#include "leveldb/cache.h"
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/table_builder.h"
#include "leveldb/write_batch.h"
#include "port/port_stdcxx.h"
#include "util/mutex_lock.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace simple_leveldb {

	struct db_impl::compaction_state {
		struct output {
			uint64_t     number;
			uint64_t     file_size;
			internal_key smallest, largest;
		};

		compaction* const      compaction;
		sequence_number        smallest_snapshot;
		core::vector< output > outputs;
		writable_file*         outfile;
		table_builder*         builder;
		uint64_t               total_bytes;

		explicit compaction_state( class compaction* c )
				: compaction( c )
				, smallest_snapshot( 0 )
				, outfile( nullptr )
				, builder( nullptr )
				, total_bytes( 0 ) {}

		output* current_output() { return &outputs[ outputs.size() - 1 ]; }
	};

}// namespace simple_leveldb

namespace simple_leveldb {

	db::~db() = default;

	status db::Open( const options& options, const core::string& name, db** dbptr ) {
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
			, shutting_down_( false )
			, background_work_finished_signal_( &mtx_ )
			, db_lock_( nullptr )
			, mem_( nullptr )
			, log_file_( nullptr )
			, logfile_number_( 0 )
			, log_( nullptr )
			, versions_( new version_set( dbname_, &options_, table_cache_, &internal_comparator_ ) ) {}

	db_impl::~db_impl() {}

	struct db_impl::writer {
		status         s;
		write_batch*   batch;
		bool           sync;
		bool           done;
		port::cond_var cv;

		explicit writer( port::mutex* mtx )
				: batch( nullptr )
				, sync( false )
				, done( false )
				, cv( mtx ) {}
	};

	status db_impl::Put( const write_options& opt, const slice& key, const slice& value ) {
		return db::Put( opt, key, value );
	}

	status db_impl::Write( const write_options& opt, write_batch* batch ) {
	}

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

	status db_impl::RecoverLogFile( uint64_t log_number, bool last_log, bool* save_manifest,
																	version_edit* edit, sequence_number* max_sequence ) {
		mtx_.assert_held();

		env_->create_dir( dbname_ );
		assert( db_lock_ == nullptr );
		status s = env_->lock_file( lock_file_name( dbname_ ), &db_lock_ );
		if ( !s.is_ok() ) {
			return s;
		}

		if ( !env_->file_exists( current_file_name( dbname_ ) ) ) {
			if ( options_.create_if_missing ) {
				Log( options_.info_log, "Creating DB %s since it was missing", dbname_.c_str() );
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

		sequence_number              max_seq{ 0 };
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
		file_type                type;
		core::vector< uint64_t > logs;
		for ( auto& file: filenames ) {
			if ( parse_file_name( file, &number, &type ) ) {
				expected.erase( number );
				if ( type == file_type::kLogFile && ( ( number >= min_log ) || ( number == prev_log ) ) ) {
					logs.emplace_back( number );
				}
			}
		}
		if ( !expected.empty() ) {
			char buf[ 50 ];
			core::snprintf( buf, sizeof buf, "%d missing files; e.g.",
											static_cast< int32_t >( expected.size() ) );
			return status::corruption( buf, table_file_name( dbname_, *( expected.begin() ) ) );
		}

		core::sort( logs.begin(), logs.end() );
		for ( int32_t i = 0; auto& log: logs ) {
			s = RecoverLogFile( log, ( i++ == logs.size() - 1 ), save_manifest, edit, &max_seq );
			if ( !s.is_ok() ) {
				return s;
			}
			versions_->mark_file_number_used( log );
		}

		if ( versions_->last_sequence() < max_seq ) {
			versions_->set_last_sequence( max_seq );
		}
		return status::ok();
	}

	void db_impl::MaybeScheduleCompaction() {
		mtx_.assert_held();

		if ( background_compaction_scheduled_ ) {
			// already scheduled
		} else if ( shutting_down_.load( core::memory_order_acquire ) ) {
			//
		} else if ( !bg_error_.is_ok() ) {
			//
		} else if ( imm_ == nullptr && manual_compaction_ == nullptr && !versions_->needs_compaction() ) {
			//
		} else {
			background_compaction_scheduled_ = true;
			env_->schedule( db_impl::bg_work, this );
		}
	}

	void db_impl::bg_work( void* db ) {
		reinterpret_cast< db_impl* >( db )->background_call();
	}

	void db_impl::background_call() {
		MutexLock lock( &mtx_ );
		assert( background_compaction_scheduled_ );

		if ( shutting_down_.load( core::memory_order_acquire ) ) {
			//
		} else if ( !bg_error_.is_ok() ) {
			//
		} else {
			background_compaction();
		}

		background_compaction_scheduled_ = false;
		MaybeScheduleCompaction();
		background_work_finished_signal_.signal_all();
	}

	void db_impl::background_compaction() {
		mtx_.assert_held();

		if ( imm_ != nullptr ) {
			compact_mem_table();
			return;
		}

		compaction*  c;
		bool         is_manual = ( manual_compaction_ != nullptr );
		internal_key manual_end;
		if ( is_manual ) {
			manual_compaction* m = manual_compaction_;
			c                    = versions_->compact_range( m->level, m->begin, m->end );
			m->done              = ( c == nullptr );
			if ( c != nullptr ) {
				manual_end = c->input( 0, c->num_input_files( 0 ) - 1 )->largest;
			}
			Log( options_.info_log, "Manual compaction at level-%d from %s .. %s; will stop at %s\n",
					 m->level, ( m->begin ? m->begin->debug_string().c_str() : "(begin)" ),
					 ( m->end ? m->begin->debug_string().c_str() : "(end)" ),
					 ( m->done ? "(end)" : manual_end.debug_string().c_str() ) );
		} else {
			c = versions_->pick_compaction();
		}

		status s;
		if ( c == nullptr ) {
			//
		} else if ( !is_manual && c->is_trivial_move() ) {
			assert( c->num_input_files( 0 ) == 1 );
			file_meta_data* f = c->input( 0, 0 );
			c->edit()->remove_file( c->level(), f->number );
			c->edit()->add_file( c->level(), f->number, f->file_size, f->smallest, f->largest );
			s = versions_->log_any_apply( c->edit(), &mtx_ );
			if ( !s.is_ok() ) {
				record_background_error( s );
			}
			version_set::level_summary_storage tmp;
			Log( options_.info_log, "Moved #%lld to level-%d %lld bytes %s: %s\n",
					 static_cast< unsigned long long >( f->number ), c->level() + 1,
					 static_cast< unsigned long long >( f->file_size ),
					 s.to_string().c_str(), versions_->level_summary( &tmp ) );
		} else {
			compaction_state* compact = new compaction_state( c );
			s                         = do_compaction_work( compact );
			if ( !s.is_ok() ) {
				record_background_error( s );
			}
			cleanup_compaction( compact );
			c->release_inputs();
			RemoveObsoleteFiles();
		}
		delete c;
		// todo!
	}

	template < class T, class V >
	static void clip_to_range( T* ptr, V minvalue, V maxvalue ) {
		if ( static_cast< V >( *ptr ) > maxvalue ) *ptr = maxvalue;
		if ( static_cast< V >( *ptr ) < minvalue ) *ptr = minvalue;
	}

	options sanitize_options( const core::string& dbname, const internal_key_comparator* icmp,
														const internal_filter_policy* i_policy, const options& src ) {
		options result       = src;
		result.comparator    = icmp;
		result.filter_policy = ( src.filter_policy != nullptr ) ? i_policy : nullptr;
		clip_to_range( &result.max_open_files, 64 + kNumNonTableCacheFiles, 50000 );
		clip_to_range( &result.write_buffer_size, 64 << 10, 1 << 30 );
		clip_to_range( &result.max_file_size, 1 << 20, 1 << 30 );
		clip_to_range( &result.block_size, 1 << 10, 4 << 20 );

		if ( result.info_log == nullptr ) {
			src.env->create_dir( dbname );
			src.env->rename_file( info_log_file_name( dbname ), old_info_log_file_name( dbname ) );
			status s = src.env->new_logger( info_log_file_name( dbname ), &result.info_log );
			if ( !s.is_ok() ) {
				result.info_log = nullptr;
			}
		}
		if ( result.block_cache == nullptr ) {
			result.block_cache = new_lru_cache( 8 << 20 );
		}
		return result;
	}

}// namespace simple_leveldb
