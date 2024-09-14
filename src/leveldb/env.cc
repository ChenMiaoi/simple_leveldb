#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstdarg>
#include <cstdint>
#include <string>

namespace simple_leveldb {

	env::~env()                               = default;
	sequential_file::~sequential_file()       = default;
	random_access_file::~random_access_file() = default;
	writable_file::~writable_file()           = default;
	logger::~logger()                         = default;
	file_lock::~file_lock()                   = default;

	status env::new_appendable_file( const core::string& fname, writable_file** result ) {
		return status::not_supported( "new_appendable_file", fname );
	}

	status env::remove_file( const core::string& filename ) { return delete_file( filename ); }
	status env::delete_file( const core::string& filename ) { return remove_file( filename ); }

	status env::remove_dir( const core::string& dirname ) { return delete_dir( dirname ); }
	status env::delete_dir( const core::string& dirname ) { return remove_dir( dirname ); }

	void Log( logger* info_log, const char* format, ... ) {
		if ( info_log != nullptr ) {
			::va_list ap;
			va_start( ap, format );
			info_log->logv( format, ap );
			va_end( ap );
		}
	}

	static status do_write_string_to_file( env* env, const slice& data, const core::string& fname, bool should_sync ) {
		writable_file* file;
		status         s = env->new_writable_file( fname, &file );
		if ( !s.is_ok() ) {
			return s;
		}
		s = file->append( data );
		if ( s.is_ok() && should_sync ) {
			s = file->sync();
		}
		if ( s.is_ok() ) {
			s = file->close();
		}
		delete file;
		if ( !s.is_ok() ) {
			env->remove_file( fname );
		}
		return s;
	}

	status read_file_to_string( env* env, const core::string& fname, core::string* data ) {
		data->clear();
		sequential_file* file;
		status           s = env->new_sequential_file( fname, &file );
		if ( !s.is_ok() ) {
			return s;
		}
		static const int32_t kBufferSize = 8192;
		char*                space       = new char[ kBufferSize ];
		while ( true ) {
			slice fragment;
			s = file->read( kBufferSize, &fragment, space );
			if ( !s.is_ok() ) {
				break;
			}
			data->append( fragment.data(), fragment.size() );
			if ( fragment.empty() ) {
				break;
			}
		}
		delete[] space;
		delete file;
		return s;
	}

	status write_string_to_file( env* env, const slice& data, const core::string& fname ) {
		return do_write_string_to_file( env, data, fname, false );
	}

	status write_string_to_file_sync( env* env, const slice& data, const core::string& fname ) {
		return do_write_string_to_file( env, data, fname, true );
	}

}// namespace simple_leveldb
