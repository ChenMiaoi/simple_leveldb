#include "leveldb/env.h"
#include "leveldb/status.h"

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

}// namespace simple_leveldb
