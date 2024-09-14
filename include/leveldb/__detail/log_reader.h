#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H

#include "leveldb/__detail/log_format.h"
#include "leveldb/env.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include <cstddef>
#include <cstdint>
#include <string>

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

	public:
		reader( sequential_file* file, reporter* reporter, bool checksum, uint64_t initial_offset );
		reader( const reader& )            = delete;
		reader& operator=( const reader& ) = delete;
		~reader();

	public:
		bool read_record( slice* record, core::string* scratch );

	private:
		enum {
			kEof       = kMaxRecordType + 1,
			kBadRecord = kMaxRecordType + 2,
		};
		uint32_t read_physical_record( slice* result );
		bool     skip_to_initial_block();
		void     report_drop( uint64_t bytes, const status& reason );
		void     report_corruption( uint64_t bytes, const char* reason );
	};

}// namespace simple_leveldb::log

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_READER_H
