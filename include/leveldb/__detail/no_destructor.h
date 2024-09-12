#ifndef STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_NO_DESTRUCTOR_H
#define STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_NO_DESTRUCTOR_H

#include <type_traits>
#include <utility>

namespace simple_leveldb {

	namespace core = std;

	template < typename InstanceType >
	class no_destructor {
	private:
		typename core::aligned_storage_t< sizeof( InstanceType ) > instance_storage_;

	public:
		template < typename... ConstructorArgTypes >
		explicit no_destructor( ConstructorArgTypes&&... constructor_args ) {
			static_assert( sizeof( instance_storage_ ) >= sizeof( InstanceType ),
										 "instance_storage_ is not large enough to hold the instance" );
			static_assert(
				alignof( decltype( instance_storage_ ) ) >= alignof( InstanceType ),
				"instance_storage_ does not meet the instance's alignment requirement" );

			new ( &instance_storage_ )
				InstanceType( core::forward< ConstructorArgTypes >( constructor_args )... );
		}

		~no_destructor() = default;

		no_destructor( const no_destructor& )            = delete;
		no_destructor& operator=( const no_destructor& ) = delete;

		InstanceType* get() {
			return reinterpret_cast< InstanceType* >( &instance_storage_ );
		}
	};

}// namespace simple_leveldb

#endif//! STORAGE_SIMPEL_LEVELDB_INCLUDE_DETAIL_NO_DESTRUCTOR_H
