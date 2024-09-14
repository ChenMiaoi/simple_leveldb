#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_ENV_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_ENV_H


#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace simple_leveldb {

#define VIRTUAL_DEFAULT_DISABLE_COPY( type_name )     \
	type_name()                              = default; \
	type_name( const type_name& )            = delete;  \
	type_name& operator=( const type_name& ) = delete;  \
	virtual ~type_name()

	class file_lock;
	class logger;
	class random_access_file;
	class sequential_file;
	class writable_file;

	class env {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( env );

	public:
		static env* Default();

		virtual status new_sequential_file( const core::string& fname, sequential_file** result )       = 0;
		virtual status new_random_access_file( const core::string& fname, random_access_file** result ) = 0;
		virtual status new_writable_file( const core::string& fname, writable_file** result )           = 0;
		virtual status new_appendable_file( const core::string& fname, writable_file** result );
		virtual bool   file_exists( const core::string& fname )                                      = 0;
		virtual status get_children( const core::string& dir, core::vector< core::string >* result ) = 0;
		virtual status remove_file( const core::string& fname );
		[[deprecated( "Modern Env implementations should override RemoveFile instead." )]]
		virtual status delete_file( const core::string& fname );
		virtual status create_dir( const core::string& dir_name ) = 0;
		virtual status remove_dir( const core::string& dir_name );
		[[deprecated( "Modern Env implementations should override RemoveDir instead." )]]
		virtual status   delete_dir( const core::string& dir_name );
		virtual status   get_file_size( const core::string& fname, uint64_t* file_size )    = 0;
		virtual status   rename_file( const core::string& src, const core::string& target ) = 0;
		virtual status   lock_file( const core::string& fname, file_lock** lock )           = 0;
		virtual status   unlock_file( file_lock* lock )                                     = 0;
		virtual void     schedule( core::function< void( void* ) >&& func, void* args )     = 0;
		virtual void     start_thread( core::function< void( void* ) >&& func, void* args ) = 0;
		virtual status   get_test_directory( core::string* path )                           = 0;
		virtual status   new_logger( const core::string& fname, logger** result )           = 0;
		virtual uint64_t now_micros()                                                       = 0;
		virtual void     sleep_for_microseconds( int32_t micros )                           = 0;
	};// end env


	class sequential_file {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( sequential_file );

		virtual status read( size_t n, slice* result, char* scratch ) = 0;
		virtual status skip( uint64_t n )                             = 0;
	};// end sequential file

	class random_access_file {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( random_access_file );

		virtual status read( uint64_t offset, size_t n, slice* result, char* scratch ) const = 0;
	};

	class writable_file {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( writable_file );

		virtual status append( const slice& data ) = 0;
		virtual status close()                     = 0;
		virtual status flush()                     = 0;
		virtual status sync()                      = 0;
	};

	class logger {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( logger );

		virtual void logv( const char* format, core::va_list ap ) = 0;
	};

	class file_lock {
	public:
		VIRTUAL_DEFAULT_DISABLE_COPY( file_lock );
	};

	void Log( logger* info_log, const char* format, ... )
#if defined( __GNUC__ ) || defined( __clang__ )
		__attribute__( ( __format__( __printf__, 2, 3 ) ) )
#endif
		;

	status read_file_to_string( env* env, const core::string& fname, core::string* data );
	status read_file_to_string_sync( env* env, core::string& fname, core::string* data );
	status write_string_to_file( env* env, const slice& data, const core::string& fname );
	status write_string_to_file_sync( env* env, const slice& data, const core::string& fname );

	class env_wrapper : public env {
	private:
		env* target_;
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_ENV_H
