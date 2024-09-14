#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/filename.h"
#include "leveldb/__detail/log_reader.h"
#include "leveldb/__detail/log_write.h"
#include "leveldb/__detail/version_edit.h"
#include "leveldb/__detail/version_set.h"
#include "leveldb/env.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace simple_leveldb {

	static size_t target_file_size( const options* option ) {
		return option->max_file_size;
	}

	version::version( version_set* vset )
			: vset_( vset )
			, next_( nullptr )
			, prev_( nullptr )
			, refs_( 0 )
			, file_to_compact_( nullptr )
			, file_to_compact_level_( -1 )
			, compaction_score_( -1 )
			, compaction_level_( -1 ) {}

	version::~version() {
		assert( refs_ == 0 );

		prev_->next_ = next_;
		next_->prev_ = prev_;

		for ( int32_t level = 0; level < config::kNumLevels; level++ ) {
			for ( size_t i = 0; i < files_[ level ].size(); i++ ) {
				file_meta_data* f = files_[ level ][ i ];
				assert( f->refs > 0 );
				f->refs--;
				if ( f->refs <= 0 ) {
					delete f;
				}
			}
		}
	}

	void version::ref() {
		++refs_;
	}

	void version::un_ref() {
		assert( this != &vset_->dummy_versions_ );
		assert( refs_ >= 1 );
		--refs_;
		if ( refs_ == 0 ) {
			delete this;
		}
	}

}// namespace simple_leveldb

namespace simple_leveldb {

	version_set::version_set( const core::string& dbname, const options* option,
														table_cache* table_cache, const internal_key_comparator* cmp )
			: env_( option->env )
			, dbname_( dbname )
			, options_( option )
			, table_cache_( table_cache )
			, icmp_( *cmp )
			, next_file_number_( 2 )
			, manifest_file_number_( 0 )
			, last_sequence_( 0 )
			, log_number_( 0 )
			, prev_log_number_( 0 )
			, descriptor_file_( nullptr )
			, descriptor_log_( nullptr )
			, dummy_versions_( this )
			, current_( nullptr ) {
		append_version( new version( this ) );
	}

	static double max_bytes_for_level( const options* options, int32_t level ) {
		double result = 10. * 1048576.0;
		while ( level-- > 1 ) {
			result *= 10;
		}
		return result;
	}

	static int64_t total_file_size( const core::vector< file_meta_data* >& files ) {
		int64_t sum = 0;
		for ( const auto file: files ) {
			sum += file->file_size;
		}
		return sum;
	}


	class version_set::builder {
	private:
		struct by_smallest_key {
			const internal_key_comparator* internal_comparator;

			bool operator()( file_meta_data* f1, file_meta_data* f2 ) const {
				int32_t r = internal_comparator->compare( f1->smallest, f2->smallest );
				if ( r != 0 ) {
					return ( r < 0 );
				} else {
					return ( f1->number < f2->number );
				}
			}
		};

		using file_set = core::set< file_meta_data*, by_smallest_key >;

		struct level_state {
			core::set< uint64_t > deleted_files;
			file_set*             added_files;
		};

	private:
		version_set* vset_;
		version*     base_;
		level_state  levels_[ config::kNumLevels ];

	public:
		builder( version_set* vset, version* base )
				: vset_( vset )
				, base_( base ) {
			base_->ref();
			by_smallest_key cmp;
			cmp.internal_comparator = &vset->icmp_;
			for ( int32_t level = 0; level < config::kNumLevels; level++ ) {
				levels_[ level ].added_files = new file_set( cmp );
			}
		}

		~builder() {
			for ( int32_t level = 0; level < config::kNumLevels; level++ ) {
				const file_set*                 added = levels_[ level ].added_files;
				core::vector< file_meta_data* > to_unref;
				to_unref.reserve( added->size() );
				for ( auto add: *added ) {
					to_unref.emplace_back( add );
				}
				delete added;

				for ( auto& f: to_unref ) {
					f->refs--;
					if ( f->refs <= 0 ) {
						delete f;
					}
				}
			}
			base_->un_ref();
		}

	public:
		void apply( const version_edit* edit ) {
			for ( auto [ level, key ]: edit->compact_pointers_ ) {
				vset_->compact_pointer_[ level ] = key.encode().to_string();
			}

			for ( const auto& [ level, number ]: edit->deleted_files_ ) {
				levels_[ level ].deleted_files.insert( number );
			}

			for ( auto [ level, f ]: edit->new_files_ ) {
				file_meta_data* file = new file_meta_data( f );
				file->refs           = 1;
				file->allowed_seeks  = static_cast< int32_t >( file->file_size / 16384U );
				if ( file->allowed_seeks < 100 ) file->allowed_seeks = 100;

				levels_[ level ].deleted_files.erase( file->number );
				levels_[ level ].added_files->insert( file );
			}
		}

		void save_to( version* v ) {
			// todo!
		}
	};

	status version_set::log_any_apply( version_edit* edit, port::mutex* mtx ) {
		if ( edit->has_log_number_ ) {
			assert( edit->log_number_ > log_number_ );
			assert( edit->log_number_ < next_file_number_ );
		} else {
			edit->set_log_number( log_number_ );
		}

		if ( !edit->has_prev_log_number_ ) {
			edit->set_prev_log_number( prev_log_number_ );
		}

		edit->set_next_file( next_file_number_ );
		edit->set_last_sequence( last_sequence_ );

		version* v = new version( this );
		{
			builder b( this, current_ );
			b.apply( edit );
			b.save_to( v );
		}
		finalize( v );

		core::string new_manifest_file;
		status       s;
		if ( descriptor_log_ == nullptr ) {
			assert( descriptor_file_ == nullptr );
			new_manifest_file = descriptor_file_name( dbname_, manifest_file_number_ );
			s                 = env_->new_writable_file( new_manifest_file, &descriptor_file_ );
			if ( s.is_ok() ) {
				descriptor_log_ = new log::writer( descriptor_file_ );
				s               = write_snap_shot( descriptor_log_ );
			}
		}

		{
			mtx->unlock();
			if ( s.is_ok() ) {
				core::string record;
				edit->encode_to( &record );
				s = descriptor_log_->add_record( record );
				if ( s.is_ok() ) {
					s = descriptor_file_->sync();
				}
				if ( !s.is_ok() ) {
					Log( options_->info_log, "MANIFEST write: %s\n", s.to_string().c_str() );
				}
			}

			if ( s.is_ok() && !new_manifest_file.empty() ) {
				s = set_current_file( env_, dbname_, manifest_file_number_ );
			}

			mtx->lock();
		}

		if ( s.is_ok() ) {
			append_version( v );
			log_number_      = edit->log_number_;
			prev_log_number_ = edit->prev_log_number_;
		} else {
			delete v;
			if ( !new_manifest_file.empty() ) {
				delete descriptor_log_;
				delete descriptor_file_;
				descriptor_log_  = nullptr;
				descriptor_file_ = nullptr;
				env_->remove_file( new_manifest_file );
			}
		}

		return s;
	}

	void version_set::set_last_sequence( uint64_t s ) {
		assert( s >= last_sequence_ );
		last_sequence_ = s;
	}

	status version_set::recover( bool* save_manifest ) {
		struct log_reporter : public log::reader::reporter {
			status* s;

			void corruption( size_t bytes, const status& status ) override {
				if ( this->s->is_ok() ) *this->s = status;
			}
		};

		core::string current;
		status       s = read_file_to_string( env_, current_file_name( dbname_ ), &current );
		if ( !s.is_ok() ) {
			return s;
		}
		if ( current.empty() || current[ current.size() - 1 ] != '\n' ) {
			return status::corruption( "CURRENT file does not end with newline" );
		}
		current.resize( current.size() - 1 );

		core::string     dscname = dbname_ + "/" + current;
		sequential_file* file;
		s = env_->new_sequential_file( dscname, &file );
		if ( !s.is_ok() ) {
			if ( s.is_not_found() ) {
				return status::corruption( "CURRENT points to a non-existent file", s.to_string() );
			}
			return s;
		}

		bool     have_log_number      = false;
		bool     have_prev_log_number = false;
		bool     have_next_file       = false;
		bool     have_last_sequence   = false;
		uint64_t next_file            = 0;
		uint64_t last_sequence        = 0;
		uint64_t log_number           = 0;
		uint64_t prev_log_number      = 0;
		builder  b( this, current_ );
		int32_t  read_records = 0;

		{
			log_reporter reporter;
			reporter.s = &s;
			log::reader reader( file, &reporter, true, 0 );

			slice        record;
			core::string scratch;
			while ( reader.read_record( &record, &scratch ) && s.is_ok() ) {
				++read_records;
				version_edit edit;
				s = edit.decode_from( record );
				if ( s.is_ok() ) {
					if ( edit.has_comparator_ && edit.comparator_ != icmp_.user_comparator()->name() ) {
						s = status::invalid_argument(
							edit.comparator_ + " dose not match exsisting comparator ",
							icmp_.user_comparator()->name() );
					}
				}
				if ( s.is_ok() ) {
					b.apply( &edit );
				}
				if ( edit.has_log_number_ ) {
					log_number      = edit.log_number_;
					have_log_number = true;
				}
				if ( edit.has_prev_log_number_ ) {
					prev_log_number      = edit.prev_log_number_;
					have_prev_log_number = true;
				}
				if ( edit.has_next_file_number_ ) {
					next_file      = edit.next_file_number_;
					have_next_file = true;
				}
				if ( edit.has_last_sequence_ ) {
					last_sequence      = edit.last_sequence_;
					have_last_sequence = true;
				}
			}
		}

		delete file;
		file = nullptr;

		if ( s.is_ok() ) {
			if ( !have_next_file ) {
				s = status::corruption( "no meta-nextfile entry in descriptor" );
			} else if ( !have_log_number ) {
				s = status::corruption( "no meta-lognumber entry in descriptor" );
			} else if ( !have_last_sequence ) {
				s = status::corruption( "no last-sequence-number entry in descriptor" );
			}
		}

		if ( !have_prev_log_number ) {
			prev_log_number = 0;
		}

		mark_file_number_used( prev_log_number );
		mark_file_number_used( log_number );

		if ( s.is_ok() ) {
			version* v = new version( this );
			b.save_to( v );
			finalize( v );
			append_version( v );
			manifest_file_number_ = next_file;
			next_file_number_     = next_file + 1;
			last_sequence_        = last_sequence;
			log_number_           = log_number;
			prev_log_number_      = prev_log_number;

			if ( reuse_manifest( dscname, current ) ) {
				// no need
			} else {
				*save_manifest = true;
			}
		} else {
			core::string error = s.to_string();
			Log( options_->info_log, "Error recovering version set with % d records : %s ",
					 read_records, error.c_str() );
		}
		return s;
	}

	void version_set::add_live_files( core::set< uint64_t >* live ) {
		for ( version* v = dummy_versions_.next_; v != &dummy_versions_; v = v->next_ ) {
			for ( auto& files: v->files_ ) {
				for ( auto file: files ) {
					live->insert( file->number );
				}
			}
		}
	}

	void version_set::mark_file_number_used( uint64_t number ) {
		if ( next_file_number_ <= number ) {
			next_file_number_ = number + 1;
		}
	}

	bool version_set::needs_compaction() const {
		version* v = current_;
		return ( v->compaction_score_ >= 1 ) || ( v->file_to_compact_ != nullptr );
	}

	void version_set::finalize( version* v ) {
		int32_t best_level = -1;
		double  best_score = -1;

		for ( int32_t level = 0; level < config::kNumLevels - 1; level++ ) {
			double score;
			if ( level == 0 ) {
				score = v->files_[ level ].size() / static_cast< double >( config::kL0_CompactionTrigger );
			} else {
				const uint64_t level_bytes = total_file_size( v->files_[ level ] );
				score =
					static_cast< double >( level_bytes ) / max_bytes_for_level( options_, level );

				if ( score > best_score ) {
					best_level = level;
					best_score = score;
				}
			}
		}

		v->compaction_level_ = best_level;
		v->compaction_score_ = best_score;
	}

	status version_set::write_snap_shot( log::writer* log ) {
		version_edit edit;
		edit.set_comparator_name( icmp_.user_comparator()->name() );

		for ( int32_t level = 0; auto cp: compact_pointer_ ) {
			if ( !cp.empty() ) {
				internal_key key;
				key.decode_from( cp );
				edit.set_compact_pointer( level, key );
			}
			level++;
		}

		for ( int32_t level = 0; auto& files: current_->files_ ) {
			for ( auto file: files ) {
				edit.add_file( level, file->number, file->file_size, file->smallest, file->largest );
			}
			level++;
		}

		core::string record;
		edit.encode_to( &record );
		return log->add_record( record );
	}


	void version_set::append_version( version* v ) {
		assert( v->refs_ == 0 );
		assert( v != current_ );
		if ( current_ != nullptr ) {
			current_->un_ref();
		}
		current_ = v;
		v->ref();

		v->prev_        = dummy_versions_.prev_;
		v->next_        = &dummy_versions_;
		v->prev_->next_ = v;
		v->next_->prev_ = v;
	}

	bool version_set::reuse_manifest( const core::string& dscname, const core::string& dscbase ) {
		if ( !options_->reuse_logs ) {
			return false;
		}
		file_type manifest_type;
		uint64_t  manifest_number;
		uint64_t  manifest_size;
		if ( !parse_file_name( dscbase, &manifest_number, &manifest_type ) ||
				 manifest_type != file_type::kDescriptorFile ||
				 !env_->get_file_size( dscname, &manifest_size ).is_ok() ||
				 manifest_size >= target_file_size( options_ ) ) {
			return false;
		}

		assert( descriptor_file_ == nullptr );
		assert( descriptor_log_ == nullptr );
		status r = env_->new_appendable_file( dscname, &descriptor_file_ );
		if ( !r.is_ok() ) {
			Log( options_->info_log, "Reuse MANIFEST: %s\n", r.to_string().c_str() );
			assert( descriptor_file_ != nullptr );
			return false;
		}

		Log( options_->info_log, "Reusing MANIFEST %s\n", dscname.c_str() );
		descriptor_log_       = new log::writer( descriptor_file_, manifest_size );
		manifest_file_number_ = manifest_number;

		return true;
	}

}// namespace simple_leveldb
