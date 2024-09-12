#include "fcntl.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "port/port_stdcxx.h"
#include "sys/mman.h"
#include "sys/stat.h"
#include "sys/types.h"
#include "unistd.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <sys/mman.h>
#include <sys/resource.h>
#include <type_traits>
#include <utility>
#include <vector>

namespace simple_leveldb {

	int32_t g_open_read_only_file_limit = -1;

	constexpr const int32_t kDefaultMmapLimit = ( sizeof( void* ) >= 8 ) ? 1000 : 0;

	int32_t g_mmap_limit = kDefaultMmapLimit;

	constexpr const int32_t kOpenBaseFlags = O_CLOEXEC;

	constexpr const size_t kWritableFileBufferSize = 65536;

	status posix_error( const core::string& context, int32_t error_number ) {
		if ( error_number == ENOENT ) {
			return status::not_found( context, core::strerror( error_number ) );
		}
		return status::io_error( context, core::strerror( error_number ) );
	}

	class limiter {
	private:
		const int32_t        max_acquires_;
		core::atomic_int32_t acquires_allowed_;

	public:
		limiter( int32_t max_acquires )
				: max_acquires_( max_acquires )
				, acquires_allowed_( max_acquires ) {
			assert( max_acquires >= 0 );
		}
		limiter( const limiter& )            = delete;
		limiter& operator=( const limiter& ) = delete;

	public:
		bool acquire() {
			int32_t old_acquires_allowed = acquires_allowed_.fetch_sub( 1, core::memory_order_relaxed );
			if ( old_acquires_allowed > 0 ) return true;

			[[maybe_unused]]
			int32_t pre_increment_acquired_allowed = acquires_allowed_.fetch_add( 1, std::memory_order_relaxed );

			assert( pre_increment_acquired_allowed < max_acquires_ );
			return false;
		}

		void release() {
			[[maybe_unused]]
			int32_t old_acquires_allowed = acquires_allowed_.fetch_add( 1, core::memory_order_relaxed );
			assert( old_acquires_allowed < max_acquires_ );
		}
	};

	class posix_sequential_file final : public sequential_file {
	private:
		const int32_t      fd_;
		const core::string filename_;

	public:
		posix_sequential_file( core::string filename, int32_t fd )
				: fd_( fd )
				, filename_( std::move( filename ) ) {}
		~posix_sequential_file() override { ::close( fd_ ); }

	public:
		status read( size_t n, slice* result, char* scratch ) override {
			status stat;
			while ( true ) {
				::ssize_t read_size = ::read( fd_, scratch, n );
				if ( read_size < 0 ) {
					if ( errno == EINTR ) {
						continue;
					}
					stat = posix_error( filename_, errno );
					break;
				}
				*result = slice( scratch, read_size );
				break;
			}
			return stat;
		}

		status skip( uint64_t n ) override {
			if ( ::lseek( fd_, n, SEEK_CUR ) == static_cast< off_t >( -1 ) ) {
				return posix_error( filename_, errno );
			}
			return status::ok();
		}
	};

	class posix_random_access_file final : public random_access_file {
	private:
		const bool         has_permanent_fd_;
		const int32_t      fd_;
		limiter* const     fd_limiter_;
		const core::string filename_;

	public:
		posix_random_access_file( core::string filename, int32_t fd, limiter* fd_limiter )
				: has_permanent_fd_( fd_limiter->acquire() )
				, fd_( has_permanent_fd_ ? fd : -1 )
				, fd_limiter_( fd_limiter )
				, filename_( filename ) {
			if ( !has_permanent_fd_ ) {
				assert( fd_ == -1 );
				::close( fd );
			}
		}
		~posix_random_access_file() override {
			if ( has_permanent_fd_ ) {
				assert( fd_ != -1 );
				::close( fd_ );
				fd_limiter_->release();
			}
		}

	public:
		status read( uint64_t offset, size_t n, slice* result, char* scratch ) const override {
			int32_t fd = fd_;
			if ( !has_permanent_fd_ ) {
				fd = ::open( filename_.c_str(), O_RDONLY | kOpenBaseFlags );
				if ( fd < 0 ) {
					return posix_error( filename_, errno );
				}
			}
			assert( fd != -1 );

			status    stat;
			::ssize_t read_size = ::pread( fd, scratch, n, static_cast< off_t >( offset ) );
			*result             = slice( scratch, ( read_size < 0 ) ? 0 : read_size );
			if ( read_size < 0 ) {
				stat = posix_error( filename_, errno );
			}
			if ( !has_permanent_fd_ ) {
				assert( fd != fd_ );
				::close( fd );
			}
			return stat;
		}
	};

	class posix_mmap_readable_file final : public random_access_file {
	private:
		char* const        mmap_base_;
		const size_t       length_;
		limiter* const     mmap_limiter_;
		const core::string filename_;

	public:
		posix_mmap_readable_file( core::string filename, char* mmap_base, size_t length, limiter* mmap_limiter )
				: mmap_base_( mmap_base )
				, length_( length )
				, mmap_limiter_( mmap_limiter )
				, filename_( filename ) {}

		~posix_mmap_readable_file() override {
			::munmap( static_cast< void* >( mmap_base_ ), length_ );
			mmap_limiter_->release();
		}

	public:
		status read( uint64_t offset, size_t n, slice* result, char* scratch ) const override {
			if ( offset + n > length_ ) {
				*result = slice();
				return posix_error( filename_, EINVAL );
			}
			*result = slice( mmap_base_ + offset, n );
			return status::ok();
		}
	};

	class posix_writable_file final : public writable_file {
	public:
		posix_writable_file( core::string filename, int32_t fd )
				: pos_( 0 )
				, fd_( fd )
				, is_manifest_( is_manifest( filename ) )
				, filename_( filename )
				, dirname_( dir_name( filename_ ) ) {}

		~posix_writable_file() override {
			if ( fd_ >= 0 ) {
				close();
			}
		}

	public:
		status append( const slice& data ) override {
			size_t      write_size = data.size();
			const char* write_data = data.data();

			size_t copy_size = core::min( write_size, kWritableFileBufferSize - pos_ );
			::memcpy( buf_ + pos_, write_data, copy_size );
			write_data += copy_size;
			write_size -= copy_size;
			pos_ += copy_size;

			if ( write_size == 0 ) {
				return status::ok();
			}

			status stat = flush_buffer();
			if ( !stat.is_ok() ) {
				return stat;
			}

			if ( write_size < kWritableFileBufferSize ) {
				::memcpy( buf_, write_data, write_size );
				pos_ = write_size;
				return status::ok();
			}
			return write_unbuffered( write_data, write_size );
		}

		status close() override {
			status        stat         = flush_buffer();
			const int32_t close_result = ::close( fd_ );
			if ( close_result < 0 && stat.is_ok() ) {
				stat = posix_error( filename_, errno );
			}
			fd_ = -1;
			return stat;
		}

		status flush() override { return flush_buffer(); }

		status sync() override {
			status stat = sync_dir_if_manifest();
			if ( !stat.is_ok() ) {
				return stat;
			}

			stat = flush_buffer();
			if ( !stat.is_ok() ) {
				return stat;
			}

			return sync_fd( fd_, filename_ );
		}

	private:
		status flush_buffer() {
			status stat = write_unbuffered( buf_, pos_ );
			pos_        = 0;
			return stat;
		}

		status write_unbuffered( const char* data, size_t size ) {
			while ( size > 0 ) {
				::ssize_t write_result = ::write( fd_, data, size );
				if ( write_result < 0 ) {
					if ( errno == EINTR ) {
						continue;
					}
					return posix_error( filename_, errno );
				}
				data += write_result;
				size -= write_result;
			}
			return status::ok();
		}

		status sync_dir_if_manifest() {
			status stat;
			if ( !is_manifest_ ) {
				return stat;
			}

			int32_t fd = ::open( dirname_.c_str(), O_RDONLY | kOpenBaseFlags );
			if ( fd < 0 ) {
				stat = posix_error( dirname_, errno );
			} else {
				stat = sync_fd( fd, dirname_ );
				::close( fd );
			}
			return stat;
		}

		static status sync_fd( int32_t fd, const core::string& fd_path ) {
			bool sync_success = ::fdatasync( fd ) == 0;

			if ( sync_success ) {
				return status::ok();
			}
			return posix_error( fd_path, errno );
		}

		static core::string dir_name( const core::string& filename ) {
			core::string::size_type separator_pos = filename.rfind( '/' );
			if ( separator_pos == core::string::npos ) {
				return ".";
			}
			assert( filename.find( '/', separator_pos + 1 ) == core::string::npos );

			return filename.substr( 0, separator_pos );
		}

		static slice base_name( const core::string& filename ) {
			core::string::size_type separator_pos = filename.rfind( '/' );
			if ( separator_pos == core::string::npos ) {
				return filename;
			}

			assert( filename.find( '/', separator_pos + 1 ) == core::string::npos );

			return slice( filename.data() + separator_pos + 1, filename.size() - separator_pos - 1 );
		}

		static bool is_manifest( const core::string& filename ) {
			return base_name( filename ).starts_with( "MANIFEST" );
		}

	private:
		char    buf_[ kWritableFileBufferSize ];
		size_t  pos_;
		int32_t fd_;

		const bool         is_manifest_;
		const core::string filename_;
		const core::string dirname_;
	};

	class posix_file_lock : public file_lock {
	private:
		const int32_t      fd_;
		const core::string filename_;

	public:
		posix_file_lock( int32_t fd, core::string filename )
				: fd_( fd )
				, filename_( core::move( filename ) ) {}

		int32_t             fd() const { return fd_; }
		const core::string& filename() const { return filename_; }
	};

	class posix_lock_table {
	private:
		port::mutex               mtx_;
		core::set< core::string > locked_files_;

	public:
		bool insert( const core::string& fname ) {
			mtx_.lock();
			auto [ _, succeeded ] = locked_files_.insert( fname );
			mtx_.unlock();
			return succeeded;
		}
	};

	int32_t max_mmaps() { return g_mmap_limit; }
	int32_t max_open_files() {
		if ( g_open_read_only_file_limit >= 0 ) {
			return g_open_read_only_file_limit;
		}

		::rlimit rlim;
		if ( ::getrlimit( RLIMIT_NOFILE, &rlim ) ) {
			g_open_read_only_file_limit = 50;
		} else if ( rlim.rlim_cur == RLIM_INFINITY ) {
			g_open_read_only_file_limit = core::numeric_limits< int32_t >::max();
		} else {
			g_open_read_only_file_limit = rlim.rlim_cur / 5;
		}
		return g_open_read_only_file_limit;
	}

	class posix_env : public env {
	private:
		struct background_work_item {
			explicit background_work_item( core::function< void( void* ) >&& f, void* a )
					: func( core::move( f ) )
					, arg( a ) {}

			const core::function< void( void* ) > func;
			void* const                           arg;
		};

		port::mutex    background_work_mutex_;
		port::cond_var background_work_cv_;
		bool           started_background_thread_;

		core::queue< background_work_item > background_work_queue_;
		posix_lock_table                    locks_;
		limiter                             mmap_limiter_;
		limiter                             fd_limiter_;

	public:
		posix_env()
				: background_work_cv_( &background_work_mutex_ )
				, started_background_thread_( false )
				, mmap_limiter_( max_mmaps() )
				, fd_limiter_( max_open_files() ) {}

		~posix_env() override {
			static const char msg[] = "PosixEnv singleton destroyed. Unsupported behavior!\n";
			::fwrite( msg, 1, sizeof msg, stderr );
			::abort();
		}

	public:
		status new_sequential_file( const core::string& filename, sequential_file** result ) override {
			int32_t fd = ::open( filename.c_str(), O_RDONLY | kOpenBaseFlags );
			if ( fd < 0 ) {
				*result = nullptr;
				return posix_error( filename, errno );
			}

			*result = new posix_sequential_file( filename, fd );
			return status::ok();
		}

		status new_random_access_file( const core::string& filename, random_access_file** resutl ) override {
			*resutl    = nullptr;
			int32_t fd = ::open( filename.c_str(), O_RDONLY | kOpenBaseFlags );
			if ( fd < 0 ) {
				return posix_error( filename, errno );
			}

			if ( !mmap_limiter_.acquire() ) {
				*resutl = new posix_random_access_file( filename, fd, &fd_limiter_ );
				return status::ok();
			}

			uint64_t file_size;
			status   stat = get_file_size( filename, &file_size );
			if ( stat.is_ok() ) {
				void* mmap_base = ::mmap( nullptr, file_size, PROT_READ, MAP_SHARED, fd, 0 );
				if ( mmap_base != MAP_FAILED ) {
					*resutl = new posix_mmap_readable_file( filename, reinterpret_cast< char* >( mmap_base ), file_size, &mmap_limiter_ );
				} else {
					stat = posix_error( filename, errno );
				}
			}

			::close( fd );
			if ( !stat.is_ok() ) {
				mmap_limiter_.release();
			}
			return stat;
		}

		status   new_writable_file( const core::string& filename, writable_file** result ) override {}
		status   new_appendable_file( const core::string& filename, writable_file** result ) override {}
		bool     file_exists( const core::string& filename ) override {}
		status   get_children( const core::string& directory_path, core::vector< core::string >* result ) override {}
		status   remove_file( const core::string& filename ) override {}
		status   create_dir( const core::string& dirname ) override {}
		status   remove_dir( const core::string& dirname ) override {}
		status   get_file_size( const core::string& filename, uint64_t* size ) override {}
		status   rename_file( const core::string& from, const core::string& to ) override {}
		status   lock_file( const core::string& filename, file_lock** lock ) override {}
		status   unlock_file( file_lock* lock ) override {}
		void     schedule( core::function< void( void* ) >&& func, void* args ) override {}
		void     start_thread( core::function< void( void* ) >&& func, void* args ) override {}
		status   get_test_directory( core::string* path ) override {}
		status   new_logger( const core::string& fname, logger** result ) override {}
		uint64_t now_micros() override {}
		void     sleep_for_microseconds( int32_t micros ) override {}
	};

	namespace {

		template < typename EnvType >
		class singleton_env {
		private:
			typename core::aligned_storage_t< sizeof( EnvType ) > env_storage_;
			static core::atomic_bool                              env_initialized_;

		public:
			singleton_env() {
				env_initialized_.store( true, core::memory_order_relaxed );
				static_assert( sizeof( env_storage_ ) >= sizeof( EnvType ),
											 "env_storage_ will not fit the Env" );
				static_assert( alignof( decltype( env_storage_ ) ) >= alignof( EnvType ),
											 "env_storage_ does not meet the Env's alignment needs" );

				new ( &env_storage_ ) EnvType();
			}

			~singleton_env()                                 = default;
			singleton_env( const singleton_env& )            = delete;
			singleton_env& operator=( const singleton_env& ) = delete;

		public:
			env* env() { return reinterpret_cast< class env* >( &env_storage_ ); }

			static void assert_env_not_initialized() {
				assert( !env_initialized_.load( core::memory_order_relaxed ) );
			}
		};

		template < typename EnvType >
		core::atomic_bool singleton_env< EnvType >::env_initialized_;

		using posix_default_env = singleton_env< posix_env >;

	}// namespace

	env* env::Default() {
		static posix_default_env env_container;
		return env_container.env();
	}

}// namespace simple_leveldb
