#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_FORMAT_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_FORMAT_H

#include "leveldb/env.h"
#include <cstdint>

namespace simple_leveldb::log {

	enum class record_type : int32_t {
		kZeroType = 0,
		kFullType,
		kFirstType,
		kMiddleType,
		kLastType,
	};

	static const int32_t kMaxRecordType = static_cast< int32_t >( record_type::kLastType );
	static const int     kBlockSize     = 32768;
	// Header is checksum (4 bytes), length (2 bytes), type (1 byte).
	static const int kHeaderSize = 4 + 2 + 1;

}// namespace simple_leveldb::log

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_LOG_FORMAT_H
