#include "leveldb/__detail/coding.h"
#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/write_batch_internal.h"
#include "leveldb/slice.h"
#include "leveldb/write_batch.h"

// WriteBatch::rep_ :=
//    sequence: fixed64
//    count: fixed32
//    data: record[count]
// record :=
//    kTypeValue varstring varstring         |
//    kTypeDeletion varstring
// varstring :=
//    len: varint32
//    data: uint8[len]

namespace simple_leveldb {

	void write_batch::Put( const slice& key, const slice& value ) {
		write_batch_internal::set_count( this, write_batch_internal::count( this ) );
		rep_.push_back( static_cast< char >( value_type::kTypeValue ) );
		put_length_prefixd_slice( &rep_, key );
		put_length_prefixd_slice( &rep_, value );
	}

}// namespace simple_leveldb
