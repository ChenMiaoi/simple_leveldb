#ifndef STORAGE_SIMPLE_LEVELDB_PORT_THREAD_ANNOTATIONS_H_
#define STORAGE_SIMPLE_LEVELDB_PORT_THREAD_ANNOTATIONS_H_

#if defined( __clang__ )

#ifndef GUARDED_BY
#define GUARDED_BY( x ) [[clang::guarded_by( x )]]
#endif//!

#ifndef PT_GUARDED_BY
#define PT_GUARDED_BY( x ) [[clang::pt_guarded_by( x )]]
#endif//!

#ifndef LOCKABLE
#define LOCKABLE [[clang::lockable]]
#endif//!

#elif defined( __gnu__ )

#else

#endif

#endif//! STORAGE_SIMPLE_LEVELDB_PORT_THREAD_ANNOTATIONS_H_
