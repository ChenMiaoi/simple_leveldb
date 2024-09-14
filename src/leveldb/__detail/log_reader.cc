#include "leveldb/__detail/log_format.h"
#include "leveldb/__detail/log_reader.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace simple_leveldb::log {

	reader::reporter::~reporter() = default;

	reader::reader( sequential_file* file, reporter* reporter, bool checksum, uint64_t initial_offset )
			: file_( file )
			, reporter_( reporter )
			, checksum_( checksum )
			, backing_store_( new char[ kBlockSize ] )
			, buffer_()
			, eof_( false )
			, last_record_offset_( 0 )
			, end_of_buffer_offset_( 0 )
			, initial_offset_( initial_offset )
			, resyncing_( initial_offset > 0 ) {}

	reader::~reader() { delete[] backing_store_; }

	bool reader::read_record( slice* record, core::string* scratch ) {
		if ( last_record_offset_ < initial_offset_ ) {
			if ( skip_to_initial_block() ) {
				return false;
			}
		}

		scratch->clear();
		record->clear();
		bool     in_fragmented_record      = false;
		uint64_t prospective_record_offset = 0;

		slice fragment;
		while ( true ) {
			const uint32_t record_type = read_physical_record( &fragment );
			uint64_t       physical_record_offset =
				end_of_buffer_offset_ - buffer_.size() - kHeaderSize - fragment.size();

			if ( resyncing_ ) {
				switch ( static_cast< enum record_type >( record_type ) ) {
					case record_type::kMiddleType:
						continue;
					case record_type::kLastType:
						resyncing_ = false;
						continue;
					default:
						resyncing_ = false;
						break;
				}
			}

			switch ( record_type ) {
				case record_type::kFullType:
					if ( in_fragmented_record ) {
						if ( !scratch->empty() ) {
							report_corruption( scratch->size(), "partial record without end(1)" );
						}
					}
					prospective_record_offset = physical_record_offset;
					scratch->clear();
					*record             = fragment;
					last_record_offset_ = prospective_record_offset;
					return true;
				case record_type::kFirstType:
					if ( in_fragmented_record ) {
						if ( !scratch->empty() ) {
							report_corruption( scratch->size(), "partial record without end(2)" );
						}
					}
					prospective_record_offset = physical_record_offset;
					scratch->assign( fragment.data(), fragment.size() );
					in_fragmented_record = true;
					break;
				case record_type::kMiddleType:
					if ( !in_fragmented_record ) {
						report_corruption( fragment.size(), "missing start of fragmented record(1)" );
					} else {
						scratch->append( fragment.data(), fragment.size() );
					}
					break;
				case record_type::kLastType:
					if ( !in_fragmented_record ) {
						report_corruption( fragment.size(), "missing start of fragmented record(2)" );
					} else {
						scratch->append( fragment.data(), fragment.size() );
						*record             = slice( *scratch );
						last_record_offset_ = prospective_record_offset;
						return true;
					}
					break;
				case kEof:
					if ( in_fragmented_record ) {
						scratch->clear();
					}
					return false;

				case kBadRecord:
					if ( in_fragmented_record ) {
						report_corruption( scratch->size(), "error in middle of record" );
						in_fragmented_record = false;
						scratch->clear();
					}
					break;
				default:
					char buf[ 40 ];
					core::snprintf( buf, sizeof buf, "unknown record type %u", record_type );
					report_corruption( ( fragment.size() + ( in_fragmented_record ? scratch->size() : 0 ) ), buf );
					in_fragmented_record = false;
					scratch->clear();
					break;
			}
		}
	}

	uint32_t reader::read_physical_record( slice* result ) {
		while ( true ) {
			if ( buffer_.size() < kHeaderSize ) {
				if ( !eof_ ) {
					buffer_.clear();
					status s = file_->read( kBlockSize, &buffer_, backing_store_ );
					end_of_buffer_offset_ += buffer_.size();
					if ( !s.is_ok() ) {
						buffer_.clear();
						report_drop( kBlockSize, s );
						eof_ = true;
						return kEof;
					} else if ( buffer_.size() < kBlockSize ) {
						eof_ = true;
					}
					continue;
				} else {
					buffer_.clear();
					return kEof;
				}
			}

			const char*    header = buffer_.data();
			const uint32_t a      = static_cast< uint32_t >( header[ 4 ] ) & 0xff;
			const uint32_t b      = static_cast< uint32_t >( header[ 5 ] ) & 0xff;
			const uint32_t type   = header[ 6 ];
			const uint32_t length = a | ( b << 8 );
			if ( kHeaderSize + length > buffer_.size() ) {
				size_t drop_size = buffer_.size();
				buffer_.clear();
				if ( !eof_ ) {
					report_corruption( drop_size, "bad record length" );
					return kBadRecord;
				}
				return kEof;
			}

			if ( type == static_cast< uint32_t >( record_type::kZeroType ) && length == 0 ) {
				buffer_.clear();
				return kBadRecord;
			}

			if ( checksum_ ) {
				uint32_t expected_crc = simple_leveldb::crc32c::Unmask( decode_fixed32( header ) );
				uint32_t actual_crc   = simple_leveldb::crc32c::Value( header + 6, 1 + length );
				if ( actual_crc != expected_crc ) {
					size_t drop_size = buffer_.size();
					buffer_.clear();
					report_corruption( drop_size, "checksum mismatch" );
					return kBadRecord;
				}
			}

			buffer_.remove_prefix( kHeaderSize + length );

			if ( end_of_buffer_offset_ - buffer_.size() - kHeaderSize - length < initial_offset_ ) {
				result->clear();
				return kBadRecord;
			}

			*result = slice( header + kHeaderSize, length );
			return type;
		}
	}

	bool reader::skip_to_initial_block() {
		const size_t offset_in_block      = initial_offset_ % kBlockSize;
		uint64_t     block_start_location = initial_offset_ - offset_in_block;

		if ( offset_in_block > kBlockSize - 6 ) {
			block_start_location += kBlockSize;
		}

		end_of_buffer_offset_ = block_start_location;

		if ( block_start_location > 0 ) {
			status skip_status = file_->skip( block_start_location );
			if ( !skip_status.is_ok() ) {
				report_drop( block_start_location, skip_status );
				return false;
			}
		}
		return true;
	}

	void reader::report_drop( uint64_t bytes, const status& reason ) {
		if ( reporter_ != nullptr &&
				 end_of_buffer_offset_ - buffer_.size() - bytes >= initial_offset_ ) {
			reporter_->corruption( static_cast< size_t >( bytes ), reason );
		}
	}

	void reader::report_corruption( uint64_t bytes, const char* reason ) {
		report_drop( bytes, status::corruption( reason ) );
	}

}// namespace simple_leveldb::log
