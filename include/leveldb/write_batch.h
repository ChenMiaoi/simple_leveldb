#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_WRITE_BATCH_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_WRITE_BATCH_H

#include "__detail/db_format.h"
#include "leveldb/slice.h"
#include "status.h"

namespace simple_leveldb {

	class write_batch {
	private:
		core::string rep_;// See comment in write_batch.cc for the format of rep_
		friend class write_batch_internal;

	public:
		class handler {
		public:
			virtual ~handler();
			virtual void Put( const slice& key, const slice& value ) = 0;
			virtual void Delete( const slice& key )                  = 0;
		};

	public:
		write_batch();
		write_batch( const write_batch& )            = default;
		write_batch& operator=( const write_batch& ) = default;
		~write_batch();

	public:
		// Store the mapping "key->value" in the database.
		void Put( const slice& key, const slice& value );

		void Clear();
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_WRITE_BATCH_H
