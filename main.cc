#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/status.h"

#include <iostream>

int main( int, char** ) {
	simple_leveldb::db*     db;
	simple_leveldb::options option;
	option.create_if_missing = true;

	simple_leveldb::status status = simple_leveldb::db::open( option, "testdb", &db );
	if ( !status.is_ok() ) {
		std::cerr << "Unable to open/create database" << std::endl;
		return -1;
	}

	status = db->Put( simple_leveldb::write_options(), "key1", "value1" );
	if ( !status.is_ok() ) {
		std::cerr << "Error putting key-value pair into the database" << std::endl;
		return -1;
	}

	delete db;
	return 0;
}
