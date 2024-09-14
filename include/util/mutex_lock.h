// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SIMPLE_LEVELDB_UTIL_MUTEXLOCK_H
#define STORAGE_SIMPLE_LEVELDB_UTIL_MUTEXLOCK_H

#include "port/port.h"
#include "port/thread_annotations.h"

namespace simple_leveldb {

	// Helper class that locks a mutex on construction and unlocks the mutex when
	// the destructor of the MutexLock object is invoked.
	//
	// Typical usage:
	//
	//   void MyClass::MyMethod() {
	//     MutexLock l(&mu_);       // mu_ is an instance variable
	//     ... some complex code, possibly with multiple return paths ...
	//   }

	class MutexLock {
	public:
		explicit MutexLock( port::mutex* mu )
				: mu_( mu ) {
			this->mu_->lock();
		}
		~MutexLock() { this->mu_->unlock(); }

		MutexLock( const MutexLock& )            = delete;
		MutexLock& operator=( const MutexLock& ) = delete;

	private:
		port::mutex* const mu_;
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_UTIL_MUTEXLOCK_H
