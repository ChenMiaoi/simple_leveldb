#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_WRITE_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_WRITE_H

#include "leveldb/__detail/log_format.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstddef>
#include <cstdint>

namespace simple_leveldb {

	class writable_file;

	namespace log {

		class writer {
		private:
			writable_file* dest_;
			int32_t        block_offset_;
			uint32_t       type_crc_[ kMaxRecordType + 1 ];

		public:
			explicit writer( writable_file* dest );
			writer( writable_file* dest, uint64_t dest_length );
			writer( const writer& )            = delete;
			writer& operator=( const writer& ) = delete;
			~writer();

		public:
			status add_record( const slice& slice );
			status emit_physical_record( record_type type, const char* ptr, size_t length );
		};

	}// namespace log

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_WRITE_H
