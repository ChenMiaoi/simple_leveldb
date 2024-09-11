#include "leveldb/__detail/coding.h"
#include "leveldb/__detail/db_format.h"
#include "leveldb/__detail/write_batch_internal.h"

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

// rep(type value):|  8bytes  |  4bytes  |  1bytes  |  len  |  data  |  len  |  data  |
// rep(type deletion):|  8bytes  |  4bytes  |  1bytes  |  len  |  data  |

namespace simple_leveldb {

	int32_t write_batch_internal::count( const write_batch* batch ) {
		return decode_fixed32( batch->rep_.data() + 8 );
	}

	void write_batch_internal::set_count( write_batch* batch, int32_t n ) {
		encode_fixed32( &batch->rep_[ 8 ], n );
	}

	sequence_number write_batch_internal::sequence( const write_batch* batch ) {
		return sequence_number{ decode_fixed64( batch->rep_.data() ) };
	}

	void write_batch_internal::set_sequence( write_batch* batch, sequence_number seq ) {
		encode_fixed64( &batch->rep_[ 8 ], seq );
	}

}// namespace simple_leveldb
