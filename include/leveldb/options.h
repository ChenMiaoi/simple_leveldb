#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_OPTIONS_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_OPTIONS_H

#include "leveldb/comparator.h"
#include "leveldb/env.h"
#include "leveldb/filter_policy.h"

namespace simple_leveldb {

	// Options to control the behavior of a database (passed to DB::Open)
	struct options {
		// Create an Options object with default values for all fields.
		options();

		// -------------------
		// Parameters that affect behavior

		// Comparator used to define the order of keys in the table.
		// Default: a comparator that uses lexicographic byte-wise ordering
		//
		// REQUIRES: The client must ensure that the comparator supplied
		// here has the same name and orders keys *exactly* the same as the
		// comparator provided to previous open calls on the same DB.
		const comparator* comparator;

		// If true, the database will be created if it is missing.
		bool create_if_missing = false;

		// If true, an error is raised if the database already exists.
		bool error_if_exists = false;

		// If true, the implementation will do aggressive checking of the
		// data it is processing and will stop early if it detects any
		// errors.  This may have unforeseen ramifications: for example, a
		// corruption of one DB entry may cause a large number of entries to
		// become unreadable or for the entire DB to become unopenable.
		bool paranoid_checks = false;

		// Use the specified object to interact with the environment,
		// e.g. to read/write files, schedule background work, etc.
		// Default: Env::Default()
		env* env;

		logger* info_log = nullptr;

		int32_t max_open_files = 1000;

		const filter_policy* filter_policy = nullptr;
	};

	struct write_options {
		write_options() = default;

		// If true, the write will be flushed from the operating system
		// buffer cache (by calling WritableFile::Sync()) before the write
		// is considered complete.  If this flag is true, writes will be
		// slower.
		//
		// If this flag is false, and the machine crashes, some recent
		// writes may be lost.  Note that if it is just the process that
		// crashes (i.e., the machine does not reboot), no writes will be
		// lost even if sync==false.
		//
		// In other words, a DB write with sync==false has similar
		// crash semantics as the "write()" system call.  A DB write
		// with sync==true has similar crash semantics to a "write()"
		// system call followed by "fsync()".
		bool sync = false;
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_OPTIONS_H
