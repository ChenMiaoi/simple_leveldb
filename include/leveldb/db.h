#ifndef STORAGE_SIMPLE_LEVELDB_INCLUDE_DB_H
#define STORAGE_SIMPLE_LEVELDB_INCLUDE_DB_H

#include "options.h"
#include "slice.h"
#include "status.h"
#include "write_batch.h"

#include <cstdint>
#include <string>

namespace simple_leveldb {

	struct range {
		range() = default;
		range( const slice& s, const slice& l )
				: start( s )
				, limit( l ) {}

		slice start;// Included in the range
		slice limit;// Not included in the range
	};

	class db {
	public:
		static status Open( const options& options, const core::string& name, db** dbptr );

		db()                       = default;
		db( const db& )            = delete;
		db& operator=( const db& ) = delete;
		virtual ~db();

	public:
		// Set the database entry for "key" to "value".  Returns OK on success,
		// and a non-OK status on error.
		// Note: consider setting options.sync = true.
		virtual status Put( const write_options& options, const slice& key, const slice& value ) = 0;

		// Apply the specified updates to the database.
		// Returns OK on success, non-OK on failure.
		// Note: consider setting options.sync = true.
		virtual status Write( const write_options& options, write_batch* updates ) = 0;
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_INCLUDE_DB_H
