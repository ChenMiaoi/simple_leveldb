#ifndef STORAGE_SIMPLE_LEVELDB_INCLUDE_SLICE_H
#define STORAGE_SIMPLE_LEVELDB_INCLUDE_SLICE_H

#include <cassert>
#include <cstring>
#include <string>

namespace simple_leveldb {

	namespace core = std;

	class slice {
	public:
		// Create an empty slice.
		slice()
				: data_( "" )
				, size_( 0 ) {}

		// Create a slice that refers to d[0,n-1].
		slice( const char* d, size_t n )
				: data_( d )
				, size_( n ) {}

		// Create a slice that refers to the contents of "s"
		slice( const core::string& s )
				: data_( s.data() )
				, size_( s.size() ) {}

		// Create a slice that refers to s[0,strlen(s)-1]
		slice( const char* s )
				: data_( s )
				, size_( strlen( s ) ) {}

		// Intentionally copyable.
		slice( const slice& )            = default;
		slice& operator=( const slice& ) = default;

		// Return a pointer to the beginning of the referenced data
		const char* data() const { return data_; }

		// Return the length (in bytes) of the referenced data
		size_t size() const { return size_; }

		// Return true iff the length of the referenced data is zero
		bool empty() const { return size_ == 0; }

		const char* begin() const { return data(); }
		const char* end() const { return data() + size(); }

		// Return the ith byte in the referenced data.
		// REQUIRES: n < size()
		char operator[]( size_t n ) const {
			assert( n < size() );
			return data_[ n ];
		}

		// Change this slice to refer to an empty array
		void clear() {
			data_ = "";
			size_ = 0;
		}

		// Drop the first "n" bytes from this slice.
		void remove_prefix( size_t n ) {
			assert( n <= size() );
			data_ += n;
			size_ -= n;
		}

		// Return a string that contains the copy of the referenced data.
		core::string to_string() const { return core::string( data_, size_ ); }

		// Three-way comparison.  Returns value:
		//   <  0 iff "*this" <  "b",
		//   == 0 iff "*this" == "b",
		//   >  0 iff "*this" >  "b"
		int compare( const slice& b ) const;

		// Return true iff "x" is a prefix of "*this"
		bool starts_with( const slice& x ) const {
			return ( ( size_ >= x.size_ ) && ( memcmp( data_, x.data_, x.size_ ) == 0 ) );
		}

	private:
		const char* data_;
		size_t      size_;
	};

	inline bool operator==( const slice& x, const slice& y ) {
		return ( ( x.size() == y.size() ) &&
						 ( memcmp( x.data(), y.data(), x.size() ) == 0 ) );
	}

	inline bool operator!=( const slice& x, const slice& y ) { return !( x == y ); }

	inline int slice::compare( const slice& b ) const {
		const size_t min_len = ( size_ < b.size_ ) ? size_ : b.size_;
		int          r       = memcmp( data_, b.data_, min_len );
		if ( r == 0 ) {
			if ( size_ < b.size_ )
				r = -1;
			else if ( size_ > b.size_ )
				r = +1;
		}
		return r;
	}


}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_INCLUDE_SLICE_H
