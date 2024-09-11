#ifndef STORAGE_SIMPLE_LEVELDB_INCLUDE_STATUS_H
#define STORAGE_SIMPLE_LEVELDB_INCLUDE_STATUS_H

#include "leveldb/slice.h"
#include <string>
#include <utility>
namespace simple_leveldb {

	class status {
	private:
		// OK status has a null state_.  Otherwise, state_ is a new[] array
		// of the following form:
		//    state_[0..3] == length of message
		//    state_[4]    == code
		//    state_[5..]  == message
		const char* state_;

	private:
		enum class code {
			kOk,
			kNotFound,
			kCorruption,
			kNotSupported,
			kInvalidArgument,
			kIOError,
		};

		code code() const {
			return ( state_ == nullptr ) ? status::code::kOk : static_cast< enum code >( state_[ 4 ] );
		}

		status( enum code code, const slice& msg, const slice& msg2 );
		static const char* copy_state( const char* s );

	public:
		status() noexcept
				: state_( nullptr ) {}
		~status() { delete[] state_; }

		status( const status& rhs );
		status& operator=( const status& rhs );

		status( status&& rhs ) noexcept
				: state_( rhs.state_ ) { rhs.state_ = nullptr; }
		status& operator=( status&& rhs ) noexcept;

	public:
		bool is_ok() const { return state_ == nullptr; }
		bool is_not_found() const { return code() == code::kNotFound; }
		bool is_corruption() const { return code() == code::kCorruption; }
		bool is_io_error() const { return code() == code::kIOError; }
		bool is_not_supported() const { return code() == code::kNotSupported; }
		bool is_invalid_argument() const { return code() == code::kInvalidArgument; }

		// Return a string representation of this status suitable for printing.
		// Returns the string "OK" for success.
		core::string to_string() const;

	public:
		static status ok() { return {}; }
		static status not_found( const slice& msg, const slice& msg2 = slice{} ) {
			return status( status::code::kNotFound, msg, msg2 );
		}
		static status corruption( const slice& msg, const slice& msg2 = slice{} ) {
			return status( status::code::kCorruption, msg, msg2 );
		}
		static status not_supported( const slice& msg, const slice& msg2 = slice{} ) {
			return status( status::code::kNotSupported, msg, msg2 );
		}
		static status invalid_argument( const slice& msg, const slice& msg2 = slice{} ) {
			return status( status::code::kInvalidArgument, msg, msg2 );
		}
		static status io_error( const slice& msg, const slice& msg2 = slice{} ) {
			return status( status::code::kIOError, msg, msg2 );
		}
	};

	inline status::status( const status& rhs ) {
		state_ = ( rhs.state_ == nullptr ) ? nullptr : copy_state( rhs.state_ );
	}
	inline status& status::operator=( const status& rhs ) {
		if ( state_ != rhs.state_ ) {
			delete[] state_;
			state_ = ( rhs.state_ == nullptr ) ? nullptr : copy_state( rhs.state_ );
		}
		return *this;
	}
	inline status& status::operator=( status&& rhs ) noexcept {
		core::swap( state_, rhs.state_ );
		return *this;
	}

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_INCLUDE_STATUS_H
