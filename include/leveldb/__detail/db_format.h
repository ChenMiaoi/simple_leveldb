#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H

#include <cstdint>

namespace simple_leveldb {

	// Value types encoded as the last component of internal keys.
	// DO NOT CHANGE THESE ENUM VALUES: they are embedded in the on-disk
	// data structures.
	enum class value_type {
		kTypeDeletion = 0x00,
		kTypeValue    = 0x01,
	};

	// kValueTypeForSeek defines the ValueType that should be passed when
	// constructing a ParsedInternalKey object for seeking to a particular
	// sequence number (since we sort sequence numbers in decreasing order
	// and the value type is embedded as the low 8 bits in the sequence
	// number in internal keys, we need to use the highest-numbered
	// ValueType, not the lowest).
	static const value_type kValueTypeForSeek = value_type::kTypeValue;

	using sequence_number = uint64_t;

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_DB_FORMAT_H
