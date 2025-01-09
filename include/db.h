#ifndef SIMPLE_LEVELDB_DB_H
#define SIMPLE_LEVELDB_DB_H

#include "options.h"
#include "status.h"
#include "write_batch.h"
#include <config.h>

namespace simple_db {
  class SimpleDB {
    DISABLE_COPY_ASSIGN( SimpleDB );

  public:
    SimpleDB() = default;
    virtual ~SimpleDB();

  public:
    virtual Status Write( const WriteOptions& options,
                          WriteBatch*         updates ) = 0;
  };
} // namespace simple_db

#endif //! SIMPLE_LEVELDB_DB_H
