#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_WRITE_BATCH_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_WRITE_BATCH_H

#include "coding.h"
#include "db_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/write_batch.h"
#include "memory_table.h"

#include <cstddef>
#include <cstdint>

namespace simple_leveldb {

	class write_batch_internal {
	public:
		static int32_t         count( const write_batch* batch );
		static void            set_count( write_batch* batch, int32_t n );
		static sequence_number sequence( const write_batch* batch );
		static void            set_sequence( write_batch* batch, sequence_number seq );
		static slice           contents( const write_batch* batch ) { return slice( batch->rep_ ); }
		static size_t          byte_size( const write_batch* batch ) { return batch->rep_.size(); }
		static void            set_contents( write_batch* batch, const slice* contents );
		static status          insert_info( const write_batch* batch, mem_table* mem_table );
		static void            append( write_batch* dst, const write_batch* src );
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_WRITE_BATCH_H
