// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Slice is a simple structure containing a pointer into some external
// storage and a size.  The user of a Slice must ensure that the slice
// is not used after the corresponding external storage has been
// deallocated.
//
// Multiple threads can invoke const methods on a Slice without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Slice must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_SLICE_H_
#define STORAGE_LEVELDB_INCLUDE_SLICE_H_

#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>

namespace simple_db {

  /**
   * @brief Slice 类表示一个指向外部存储的指针及其大小。
   * 
   * Slice 是一个简单的结构，包含一个指向外部存储的指针和大小。
   * 使用 Slice 的用户必须确保在相应的外部存储被释放后不再使用该 Slice。
   */
  class Slice {
  private:
    const char* data_; ///< 指向外部存储的指针
    size_t      size_; ///< 外部存储的大小

  public:
    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 默认构造函数，创建一个空的 Slice 对象。
     */
    Slice()
        : data_( "" )
        , size_( 0 ) {}

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 构造函数，创建一个指向指定数据的 Slice 对象。
     *
     * 创建一个指向 d[0,n-1] 的 Slice。
     * 
     * @param [in] d 指向外部存储的指针
     * @param [in] n 外部存储的大小
     */
    Slice( const char* d, size_t n )
        : data_( d )
        , size_( n ) {}

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 构造函数，创建一个指向字符串内容的 Slice 对象。
     * 
     * 创建一个指向字符串 s 内容的 Slice。
     *
     * @param [in] s 字符串对象
     */
    Slice( const std::string& s )
        : data_( s.data() )
        , size_( s.size() ) {}

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 构造函数，创建一个指向 C 风格字符串内容的 Slice 对象。
     *
     * 创建一个指向 s[0,strlen(s)-1] 的 Slice。
     *
     * @param [in] s C 风格字符串
     */
    Slice( const char* s )
        : data_( s )
        , size_( strlen( s ) ) {}

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 默认拷贝构造函数。
     */
    Slice( const Slice& ) = default;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 默认拷贝赋值运算符。
     */
    Slice& operator=( const Slice& ) = default;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回引用数据的第 n 个字节。
     *
     * @param [in] n 索引
     * @return char 引用数据的第 n 个字节
     * @pre n < size()
     */
    char operator[]( size_t n ) const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回指向引用数据起始位置的指针。
     *
     * @return const char* 指向引用数据起始位置的指针
     */
    const char* data() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回引用数据的长度（以字节为单位）。
     *
     * @return size_t 引用数据的长度
     */
    size_t size() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 判断引用数据的长度是否为零。
     *
     * @return bool 如果引用数据的长度为零，则返回 true，否则返回 false
     */
    bool empty() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回指向引用数据起始位置的指针。
     *
     * @return const char* 指向引用数据起始位置的指针
     */
    const char* begin() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回指向引用数据结束位置的指针。
     *
     * @return const char* 指向引用数据结束位置的指针
     */
    const char* end() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 将该 Slice 设置为引用一个空数组。
     */
    void clear();

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 从该 Slice 中移除前 n 个字节。
     *
     * @param [in] n 要移除的字节数
     */
    void remove_prefix( size_t n );

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 返回引用数据的字符串副本。
     *
     * @return std::string 引用数据的字符串副本
     */
    std::string ToString() const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 三向比较函数。
     *
     * 返回值：
     *   <  0 表示 "*this" <  "b"，
     *   == 0 表示 "*this" == "b"，
     *   >  0 表示 "*this" >  "b"。
     *
     * @param [in] b 要比较的 Slice 对象
     * @return int 比较结果
     */
    int compare( const Slice& b ) const;

    /**
     * @author chenmiao (chenmiao.ku@gmail.com)
     * @date 2025-01-09
     * @brief 判断 x 是否是 *this 的前缀。
     *
     * @param [in] x 要判断的 Slice 对象
     * @return bool 如果 x 是 *this 的前缀，则返回 true，否则返回 false
     */
    bool starts_with( const Slice& x ) const;
  };

  /**
   * @author chenmiao (chenmiao.ku@gmail.com)
   * @date 2025-01-09
   * @brief 判断两个 Slice 对象是否相等。
   *
   * @param [in] x 第一个 Slice 对象
   * @param [in] y 第二个 Slice 对象
   * @return bool 如果两个 Slice 对象相等，则返回 true，否则返回 false
   */
  inline bool operator==( const Slice& x, const Slice& y ) {
    return ( ( x.size() == y.size() ) &&
             ( memcmp( x.data(), y.data(), x.size() ) == 0 ) );
  }

  /**
   * @author chenmiao (chenmiao.ku@gmail.com)
   * @date 2025-01-09
   * @brief 判断两个 Slice 对象是否不相等。
   *
   * @param [in] x 第一个 Slice 对象
   * @param [in] y 第二个 Slice 对象
   * @return bool 如果两个 Slice 对象不相等，则返回 true，否则返回 false
   */
  inline bool operator!=( const Slice& x, const Slice& y ) {
    return !( x == y );
  }

  /**
   * @author chenmiao (chenmiao.ku@gmail.com)
   * @date 2025-01-09
   * @brief 比较两个 Slice 对象。
   *
   * @param [in] b 要比较的 Slice 对象
   * @return int 比较结果
   */
  inline int Slice::compare( const Slice& b ) const {
    const size_t min_len = ( size_ < b.size_ ) ? size_ : b.size_;
    int          r       = memcmp( data_, b.data_, min_len );
    if ( r == 0 ) {
      if ( size_ < b.size_ ) r = -1;
      else if ( size_ > b.size_ )
        r = +1;
    }
    return r;
  }

} // namespace simple_db

#endif // STORAGE_LEVELDB_INCLUDE_SLICE_H_
