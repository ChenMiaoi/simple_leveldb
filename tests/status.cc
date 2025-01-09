#include <gtest/gtest.h>
#include <status.h>

namespace simple_db {
  namespace {

    TEST( StatusTest, DefaultConstructor ) {
      Status s;
      EXPECT_TRUE( s.ok() );
      EXPECT_FALSE( s.IsNotFound() );
      EXPECT_FALSE( s.IsCorruption() );
      EXPECT_FALSE( s.IsIOError() );
      EXPECT_FALSE( s.IsNotSupportedError() );
      EXPECT_FALSE( s.IsInvalidArgument() );
    }

    TEST( StatusTest, OKStatus ) {
      Status s = Status::OK();
      EXPECT_TRUE( s.ok() );
      EXPECT_EQ( s.ToString(), "OK" );
    }

    TEST( StatusTest, NotFound ) {
      Status s = Status::NotFound( "key not found" );
      EXPECT_FALSE( s.ok() );
      EXPECT_TRUE( s.IsNotFound() );
      EXPECT_EQ( s.ToString(), "NotFound: key not found" );
    }

    TEST( StatusTest, Corruption ) {
      Status s = Status::Corruption( "data corrupted" );
      EXPECT_FALSE( s.ok() );
      EXPECT_TRUE( s.IsCorruption() );
      EXPECT_EQ( s.ToString(), "Corruption: data corrupted" );
    }

    TEST( StatusTest, IOError ) {
      Status s = Status::IOError( "disk error" );
      EXPECT_FALSE( s.ok() );
      EXPECT_TRUE( s.IsIOError() );
      EXPECT_EQ( s.ToString(), "IO error: disk error" );
    }

    TEST( StatusTest, NotSupported ) {
      Status s = Status::NotSupported( "feature not supported" );
      EXPECT_FALSE( s.ok() );
      EXPECT_TRUE( s.IsNotSupportedError() );
      EXPECT_EQ( s.ToString(), "Not implemented: feature not supported" );
    }

    TEST( StatusTest, InvalidArgument ) {
      Status s = Status::InvalidArgument( "invalid parameter" );
      EXPECT_FALSE( s.ok() );
      EXPECT_TRUE( s.IsInvalidArgument() );
      EXPECT_EQ( s.ToString(), "Invalid argument: invalid parameter" );
    }

    TEST( StatusTest, CopyConstructor ) {
      Status s1 = Status::NotFound( "key not found" );
      Status s2( s1 );
      EXPECT_EQ( s1.ToString(), s2.ToString() );
    }

    TEST( StatusTest, CopyAssignment ) {
      Status s1 = Status::Corruption( "data corrupted" );
      Status s2;
      s2 = s1;
      EXPECT_EQ( s1.ToString(), s2.ToString() );
    }

    TEST( StatusTest, MoveConstructor ) {
      Status s1 = Status::IOError( "disk error" );
      Status s2( std::move( s1 ) );
      EXPECT_EQ( s2.ToString(), "IO error: disk error" );
      EXPECT_TRUE( s1.ok() );
    }

    TEST( StatusTest, MoveAssignment ) {
      Status s1 = Status::NotSupported( "feature not supported" );
      Status s2;
      s2 = std::move( s1 );
      EXPECT_EQ( s2.ToString(), "Not implemented: feature not supported" );
      EXPECT_TRUE( s1.ok() );
    }

  } // namespace
} // namespace simple_db

GTEST_API_ int main( int argc, char* argv[] ) {
  ::testing::InitGoogleTest( &argc, argv );
  return RUN_ALL_TESTS();
}
