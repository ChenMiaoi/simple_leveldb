#include "leveldb/__detail/filename.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>

namespace simple_leveldb {

	static core::string make_file_name( const core::string& dbname, uint64_t number, const char* suffix ) {
		char buf[ 100 ];
		core::snprintf( buf, sizeof buf, "/%06llu.%s",
										static_cast< unsigned long long >( number ), suffix );
		return dbname + buf;
	}

	core::string table_file_name( const core::string& dbname, uint64_t number ) {
		assert( number > 0 );
		return make_file_name( dbname, number, "ldb" );
	}

	core::string log_file_name( const core::string& dbname, uint64_t number ) {
		assert( number > 0 );
		return make_file_name( dbname, number, "log" );
	}

	core::string descriptor_file_name( const core::string& dbname, uint64_t number ) {
		assert( number > 0 );
		char buf[ 100 ];
		core::snprintf( buf, sizeof buf, "/MANIFEST-%06llu",
										static_cast< unsigned long long >( number ) );
		return dbname + buf;
	}

	core::string current_file_name( const core::string& dbname ) {
		return dbname + "/CURRENT";
	}

	core::string temp_file_name( const core::string& dbname, uint64_t number ) {
		assert( number > 0 );
		return make_file_name( dbname, number, "dbtmp" );
	}

	status set_current_file( env* env, const core::string& dbname, uint64_t descriptor_number ) {
		core::string manifest = descriptor_file_name( dbname, descriptor_number );
		slice        contents = manifest;
		assert( contents.starts_with( dbname + "/" ) );
		contents.remove_prefix( dbname.size() + 1 );
		core::string tmp = temp_file_name( dbname, descriptor_number );
		status       s   = write_string_to_file_sync( env, contents.to_string() + "\n", tmp );
		if ( s.is_ok() ) {
			s = env->rename_file( tmp, current_file_name( dbname ) );
		}
		if ( !s.is_ok() ) {
			env->remove_file( tmp );
		}
		return s;
	}

}// namespace simple_leveldb
