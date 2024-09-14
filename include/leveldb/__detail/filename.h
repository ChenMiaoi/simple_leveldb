#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_FILENAME_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_FILENAME_H

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstdint>
#include <string>

namespace simple_leveldb {

	enum class file_type {
		kLogFile,
		kDBLockFile,
		kTableFile,
		kDescriptorFile,
		kCurrentFile,
		kTempFile,
		kInfoLogFile,// Either the current one, or an old one
	};

	core::string table_file_name( const core::string& dbname, uint64_t number );

	core::string log_file_name( const core::string& dbname, uint64_t number );

	core::string descriptor_file_name( const core::string& dbname, uint64_t number );

	core::string current_file_name( const core::string& dbname );

	core::string lock_file_name( const core::string& dbname );

	core::string temp_file_name( const core::string& dbname, uint64_t number );

	core::string info_log_file_name( const core::string& dbname );

	core::string old_info_log_file_name( const core::string& dbname );

	bool parse_file_name( const core::string& filename, uint64_t* number, file_type* type );

	status set_current_file( env* env, const core::string& dbname, uint64_t descriptor_number );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_FILENAME_H
