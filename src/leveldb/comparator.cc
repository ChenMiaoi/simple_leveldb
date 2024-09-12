#include "leveldb/__detail/no_destructor.h"
#include "leveldb/comparator.h"

namespace simple_leveldb {

	comparator::~comparator() = default;

	namespace {
		class bytewise_comparator_impl : public comparator {
		public:
			bytewise_comparator_impl() = default;

			const char* name() const override {
				return "simple_leveldb.bytewise_comparator";
			}

			int32_t compare( const slice& a, const slice& b ) const override {}
			void    find_shortest_separator( core::string* start, const slice& limit ) const override {}
			void    find_short_successor( core::string* key ) const override {}
		};
	}// namespace

	const comparator* bytewise_comparator() {
		static no_destructor< bytewise_comparator_impl > singleton;
		return singleton.get();
	}

}// namespace simple_leveldb
