#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H

#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstddef>
#include <cstdint>

namespace simple_leveldb::log {

	class reader {
	public:
		class reporter {
		public:
			virtual ~reporter();
			virtual void corruption( size_t bytes, const status& status ) = 0;
		};

	private:
		sequential_file* const file_;
		reporter* const        reporter_;
		bool const             checksum_;
		char* const            backing_store_;
		slice                  buffer_;
		bool                   eof_;
		uint64_t               last_record_offset_;
		uint64_t               end_of_buffer_offset_;
		uint64_t const         initial_offset_;
		bool                   resyncing_;
	};

}// namespace simple_leveldb::log

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H
