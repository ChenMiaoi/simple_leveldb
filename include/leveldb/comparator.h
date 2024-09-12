#ifndef STORAGE_SIMPLE_LEVELDB_INCLUDE_COMPARATOR_H
#define STORAGE_SIMPLE_LEVELDB_INCLUDE_COMPARATOR_H

#include "leveldb/slice.h"
#include <cstdint>
namespace simple_leveldb {

	class comparator {
	public:
		virtual ~comparator();

		// Three-way comparison.  Returns value:
		//   < 0 iff "a" < "b",
		//   == 0 iff "a" == "b",
		//   > 0 iff "a" > "b"
		virtual int32_t     compare( const slice& a, const slice& b ) const                          = 0;
		virtual const char* name() const                                                             = 0;
		virtual void        find_shortest_separator( core::string* start, const slice& limit ) const = 0;
		virtual void        find_short_successor( core::string* key ) const                          = 0;
	};

	const comparator* bytewise_comparator();

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_INCLUDE_COMPARATOR_H
