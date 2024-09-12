#include "leveldb/comparator.h"
#include "leveldb/options.h"

#include "leveldb/env.h"

namespace simple_leveldb {

	options::options()
			: comparator( bytewise_comparator() )
			, env( env::Default() ) {}

}// namespace simple_leveldb
