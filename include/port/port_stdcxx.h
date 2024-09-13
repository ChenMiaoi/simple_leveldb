#ifndef STORAGE_SIMPEL_LEVELDB_PORT_STDCXX_H
#define STORAGE_SIMPEL_LEVELDB_PORT_STDCXX_H

#include "port/thread_annotations.h"
#include <cassert>
#include <condition_variable>
#include <mutex>

namespace simple_leveldb::port {
	namespace core = std;

	class cond_var;

	class [[clang::scoped_lockable]] mutex {
	private:
		friend class cond_var;
		core::mutex mtx_;

	public:
		mutex()                          = default;
		~mutex()                         = default;
		mutex( const mutex& )            = delete;
		mutex& operator=( const mutex& ) = delete;

	public:
		void lock() { mtx_.lock(); }
		void unlock() { mtx_.unlock(); }
		void assert_held() {}
	};

	class cond_var {
	private:
		core::condition_variable cv_;
		mutex* const             mtx_;

	public:
		explicit cond_var( mutex* mtx )
				: mtx_( mtx ) {
			assert( mtx != nullptr );
		}
		~cond_var()                            = default;
		cond_var( const cond_var& )            = delete;
		cond_var& operator=( const cond_var& ) = delete;

	public:
		void wait() {
			core::unique_lock< core::mutex > lock( mtx_->mtx_, core::adopt_lock );
			cv_.wait( lock );
			lock.release();
		}

		void signal() { cv_.notify_one(); }
		void signal_all() { cv_.notify_all(); }
	};

	inline uint32_t AcceleratedCRC32C( uint32_t crc, const char* buf, size_t size ) {
#if HAVE_CRC32C
		return ::crc32c::Extend( crc, reinterpret_cast< const uint8_t* >( buf ), size );
#else
		// Silence compiler warnings about unused arguments.
		(void) crc;
		(void) buf;
		(void) size;
		return 0;
#endif// HAVE_CRC32C
	}

}// namespace simple_leveldb::port

#endif//! STORAGE_SIMPEL_LEVELDB_PORT_STDCXX_H
