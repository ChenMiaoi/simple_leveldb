#include "leveldb/db.h"
#include "leveldb/options.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace simple_leveldb {

	db::~db() = default;

	status db::open( const options& options, const core::string& name, db** dbptr ) {
		status s;
		return s;
	}

	status db::Put( const write_options& opt, const slice& key, const slice& value ) {
		write_batch batch;
		batch.Put( key, value );
		return Write( opt, &batch );
	}

}// namespace simple_leveldb
