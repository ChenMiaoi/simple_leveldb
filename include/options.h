#ifndef SIMPLE_LEVEL_DB_OPTIONS_H
#define SIMPLE_LEVEL_DB_OPTIONS_H

/**
 * @author chenmiao (chenmiao.ku@gmail.com)
 * @date 2025-01-09
 * @brief DB内容存储在一组块中，每个块包含一系列键值对。每个块在存储到文件之前可能会被压缩。
 * 以下枚举描述了用于压缩块的压缩方法（如果有）。
 */
enum CompressionType {
  // NOTE: do not change the values of existing entries, as these are
  // part of the persistent format on disk.
  kNoCompression     = 0x0, /**< 不进行压缩 */
  kSnappyCompression = 0x1, /**< 使用Snappy压缩算法 */
  kZstdCompression   = 0x2, /**< 使用Zstandard压缩算法 */
};

/**
 * @author chenmiao (chenmiao.ku@gmail.com)
 * @date 2025-01-09
 * @brief 数据库的全局选项
 * 
 */
struct Options {
  Options();
};

/**
 * @author chenmiao (chenmiao.ku@gmail.com)
 * @date 2025-01-09
 * @brief 控制读取操作的选项
 * 
 */
struct ReadOptions {
  /**
   * @brief 如果为true，从底层存储读取的所有数据都将根据相应的校验和进行验证。
   */
  bool verify_checksums = false;

  /**
   * @brief 本次迭代读取的数据是否应缓存在内存中？
   * 对于批量扫描，调用者可能希望将此字段设置为false。
   */
  bool fill_cache = true;

  /**
   * @brief 如果"snapshot"非空，则读取指定的快照（该快照必须属于正在读取的数据库，并且必须未被释放）。
   * 如果"snapshot"为空，则使用本次读取操作开始时的隐式快照。
   */
  // const Snapshot* snapshot = nullptr;
};

/**
 * @author chenmiao (chenmiao.ku@gmail.com)
 * @date 2025-01-09
 * @brief 控制写入操作的选项
 * 
 */
struct WriteOptions {
  WriteOptions() = default;

  /**
   * @brief 如果为true，写入操作将在操作系统缓冲区缓存中刷新（通过调用WritableFile::Sync()）后才被视为完成。
   * 如果此标志为true，写入速度会变慢。
   *
   * 如果此标志为false，并且机器崩溃，可能会丢失最近的写入。
   * 请注意，如果只是进程崩溃（即机器未重启），即使sync==false，也不会丢失任何写入。
   *
   * 换句话说，sync==false的DB写入具有与"write()"系统调用类似的崩溃语义。
   * sync==true的DB写入具有与"write()"系统调用后跟"fsync()"类似的崩溃语义。
   */
  bool sync = false;
};

#endif //! SIMPLE_LEVEL_DB_OPTIONS_H
