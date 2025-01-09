#ifndef SIMPLE_LEVELDB_CONFIG_H
#define SIMPLE_LEVELDB_CONFIG_H

#define DISABLE_COPY_ASSIGN( class_name )              \
  class_name( const class_name& )            = delete; \
  class_name& operator=( const class_name& ) = delete

#endif //! SIMPLE_LEVELDB_CONFIG_H
