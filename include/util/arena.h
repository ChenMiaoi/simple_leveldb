// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SIMPLE_LEVELDB_UTIL_ARENA_H
#define STORAGE_SIMPLE_LEVELDB_UTIL_ARENA_H

#include <atomic>
#include <cassert>
#include <vector>

namespace simple_leveldb {

	namespace core = std;

	class arena {
	public:
		arena();

		arena( const arena& )            = delete;
		arena& operator=( const arena& ) = delete;

		~arena();

		// Return a pointer to a newly allocated memory block of "bytes" bytes.
		char* allocate( size_t bytes );

		// allocate_ memory with the normal alignment guarantees provided by malloc.
		char* allocate_aligned( size_t bytes );

		// Returns an estimate of the total memory usage of data allocate_d
		// by the arena.
		size_t memory_usage() const {
			return memory_usage_.load( core::memory_order_relaxed );
		}

	private:
		char* allocate_fallback( size_t bytes );
		char* allocate_new_block( size_t block_bytes );

		// Allocation state
		char*  alloc_ptr_;
		size_t alloc_bytes_remaining_;

		// Array of new[] allocate_d memory blocks
		core::vector< char* > blocks_;

		// Total memory usage of the arena.
		//
		// TODO(costan): This member is accessed via atomics, but the others are
		//               accessed without any locking. Is this OK?
		core::atomic< size_t > memory_usage_;
	};

	inline char* arena::allocate( size_t bytes ) {
		// The semantics of what to return are a bit messy if we allow
		// 0-byte allocations, so we disallow them here (we don't need
		// them for our internal use).
		assert( bytes > 0 );
		if ( bytes <= alloc_bytes_remaining_ ) {
			char* result = alloc_ptr_;
			alloc_ptr_ += bytes;
			alloc_bytes_remaining_ -= bytes;
			return result;
		}
		return allocate_fallback( bytes );
	}

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_UTIL_ARENA_H
