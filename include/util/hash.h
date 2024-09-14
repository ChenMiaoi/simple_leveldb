// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Simple hash function used for internal data structures

#ifndef STORAGE_SIMPLE_LEVELDB_UTIL_HASH_H
#define STORAGE_SIMPLE_LEVELDB_UTIL_HASH_H

#include <cstddef>
#include <cstdint>

namespace simple_leveldb {

	uint32_t Hash( const char* data, size_t n, uint32_t seed );

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_UTIL_HASH_H
