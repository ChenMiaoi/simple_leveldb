#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/filename.h"
#include "leveldb/__detail/log_reader.h"
#include "leveldb/__detail/log_write.h"
#include "leveldb/__detail/version_set.h"
#include "leveldb/options.h"
#include "leveldb/status.h"
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

namespace simple_leveldb {

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
		struct log_reporter : public log::reader::reporter {};
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

}// namespace simple_leveldb
