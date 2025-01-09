#ifndef SIMPLE_LEVELDB_STATUS_H
#define SIMPLE_LEVELDB_STATUS_H

#include <slice.h>
#include <string>
#include <utility>

namespace simple_db {
  class Status {
  private:
    /**
   * @author chenmiao (chenmiao.ku@gmail.com)
   * @date 2025-01-06
   * @brief OK status has a null state_.
   *
   * Otherwise, state_ is a new[] array of the following form:
   *  state_[0..3] == length of message
   *  state_[4]    == code
   *  state_[5..]  == message
   */
    const char* state_;

  public:
    Status() noexcept
        : state_( nullptr ) {}
    ~Status() { delete[] state_; }

    Status( const Status& rhs );
    Status& operator=( const Status& rhs );

    Status( Status&& rhs ) noexcept
        : state_( rhs.state_ ) {
      rhs.state_ = nullptr;
    };
    Status& operator=( Status&& rhs ) noexcept;

  public:
    // Return a success status.
    static Status OK() { return Status(); }

    // Return error status of an appropriate type.
    static Status NotFound( const Slice& msg, const Slice& msg2 = Slice() ) {
      return Status( kNotFound, msg, msg2 );
    }

    static Status Corruption( const Slice& msg, const Slice& msg2 = Slice() ) {
      return Status( kCorruption, msg, msg2 );
    }

    static Status NotSupported( const Slice& msg,
                                const Slice& msg2 = Slice() ) {
      return Status( kNotSupported, msg, msg2 );
    }

    static Status InvalidArgument( const Slice& msg,
                                   const Slice& msg2 = Slice() ) {
      return Status( kInvalidArgument, msg, msg2 );
    }

    static Status IOError( const Slice& msg, const Slice& msg2 = Slice() ) {
      return Status( kIOError, msg, msg2 );
    }

    bool ok() const { return ( state_ == nullptr ); }

    bool IsNotFound() const { return code() == kNotFound; }

    // Returns true iff the status indicates a Corruption errr.
    bool IsCorruption() const { return code() == kCorruption; }

    bool IsIOError() const { return code() == kIOError; }

    bool IsNotSupportedError() const { return code() == kNotSupported; }

    bool IsInvalidArgument() const { return code() == kInvalidArgument; }

    std::string ToString() const;

  private:
    enum Code {
      kOk              = 0,
      kNotFound        = 1,
      kCorruption      = 2,
      kNotSupported    = 3,
      kInvalidArgument = 4,
      kIOError         = 5
    };

    Status( Code code, const Slice& msg, const Slice& msg2 );
    static const char* CopyState( const char* s );

    Code code() const {
      return ( state_ == nullptr ) ? kOk : static_cast< Code >( state_[ 4 ] );
    }
  };

  inline Status::Status( const Status& rhs ) {
    state_ = ( rhs.state_ == nullptr ) ? nullptr : CopyState( rhs.state_ );
  }

  inline Status& Status::operator=( const Status& rhs ) {
    // The following condition catches both aliasing (when this == &rhs),
    // and the common case where both rhs and *this are ok.
    if ( state_ != rhs.state_ ) {
      delete[] state_;
      state_ = ( rhs.state_ == nullptr ) ? nullptr : CopyState( rhs.state_ );
    }
    return *this;
  }

  inline Status& Status::operator=( Status&& rhs ) noexcept {
    std::swap( state_, rhs.state_ );
    return *this;
  }
} // namespace simple_db

#endif //! SIMPLE_LEVELDB_STATUS_H
