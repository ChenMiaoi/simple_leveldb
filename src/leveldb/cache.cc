// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/cache.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <functional>

#include "leveldb/slice.h"
#include "port/thread_annotations.h"
#include "util/hash.h"
#include "util/mutex_lock.h"

namespace simple_leveldb {

	cache::~cache() {}

	namespace {

		// LRU cache implementation
		//
		// cache entries have an "in_cache" boolean indicating whether the cache has a
		// reference on the entry.  The only ways that this can become false without the
		// entry being passed to its "deleter" are via Erase(), via Insert() when
		// an element with a duplicate key is inserted, or on destruction of the cache.
		//
		// The cache keeps two linked lists of items in the cache.  All items in the
		// cache are in one list or the other, and never both.  Items still referenced
		// by clients but erased from the cache are in neither list.  The lists are:
		// - in-use:  contains the items currently referenced by clients, in no
		//   particular order.  (This list is used for invariant checking.  If we
		//   removed the check, elements that would otherwise be on this list could be
		//   left as disconnected singleton lists.)
		// - LRU:  contains the items not currently referenced by clients, in LRU order
		// Elements are moved between these lists by the Ref() and Unref() methods,
		// when they detect an element in the cache acquiring or losing its only
		// external reference.

		// An entry is a variable length heap-allocated structure.  Entries
		// are kept in a circular doubly linked list ordered by access time.
		struct lru_handle {
			void*                                         value;
			core::function< void( const slice&, void* ) > deleter;
			lru_handle*                                   next_hash;
			lru_handle*                                   next;
			lru_handle*                                   prev;
			size_t                                        charge;// TODO(opt): Only allow uint32_t?
			size_t                                        key_length;
			bool                                          in_cache;     // Whether entry is in the cache.
			uint32_t                                      refs;         // References, including cache reference, if present.
			uint32_t                                      hash;         // Hash of key(); used for fast sharding and comparisons
			char                                          key_data[ 1 ];// Beginning of key

			slice key() const {
				// next is only equal to this if the LRU handle is the list head of an
				// empty list. List heads never have meaningful keys.
				assert( next != this );

				return slice( key_data, key_length );
			}
		};

		// We provide our own simple hash table since it removes a whole bunch
		// of porting hacks and is also faster than some of the built-in hash
		// table implementations in some of the compiler/runtime combinations
		// we have tested.  E.g., readrandom speeds up by ~5% over the g++
		// 4.4.3's builtin hashtable.
		class handle_table {
		public:
			handle_table()
					: length_( 0 )
					, elems_( 0 )
					, list_( nullptr ) { resize(); }
			~handle_table() { delete[] list_; }

			lru_handle* look_up( const slice& key, uint32_t hash ) {
				return *find_pointer( key, hash );
			}

			lru_handle* insert( lru_handle* h ) {
				lru_handle** ptr = find_pointer( h->key(), h->hash );
				lru_handle*  old = *ptr;
				h->next_hash     = ( old == nullptr ? nullptr : old->next_hash );
				*ptr             = h;
				if ( old == nullptr ) {
					++elems_;
					if ( elems_ > length_ ) {
						// Since each cache entry is fairly large, we aim for a small
						// average linked list length (<= 1).
						resize();
					}
				}
				return old;
			}

			lru_handle* remove( const slice& key, uint32_t hash ) {
				lru_handle** ptr    = find_pointer( key, hash );
				lru_handle*  result = *ptr;
				if ( result != nullptr ) {
					*ptr = result->next_hash;
					--elems_;
				}
				return result;
			}

		private:
			// The table consists of an array of buckets where each bucket is
			// a linked list of cache entries that hash into the bucket.
			uint32_t     length_;
			uint32_t     elems_;
			lru_handle** list_;

			// Return a pointer to slot that points to a cache entry that
			// matches key/hash.  If there is no such cache entry, return a
			// pointer to the trailing slot in the corresponding linked list.
			lru_handle** find_pointer( const slice& key, uint32_t hash ) {
				lru_handle** ptr = &list_[ hash & ( length_ - 1 ) ];
				while ( *ptr != nullptr && ( ( *ptr )->hash != hash || key != ( *ptr )->key() ) ) {
					ptr = &( *ptr )->next_hash;
				}
				return ptr;
			}

			void resize() {
				uint32_t new_length = 4;
				while ( new_length < elems_ ) {
					new_length *= 2;
				}
				lru_handle** new_list = new lru_handle*[ new_length ];
				memset( new_list, 0, sizeof( new_list[ 0 ] ) * new_length );
				uint32_t count = 0;
				for ( uint32_t i = 0; i < length_; i++ ) {
					lru_handle* h = list_[ i ];
					while ( h != nullptr ) {
						lru_handle*  next = h->next_hash;
						uint32_t     hash = h->hash;
						lru_handle** ptr  = &new_list[ hash & ( new_length - 1 ) ];
						h->next_hash      = *ptr;
						*ptr              = h;
						h                 = next;
						count++;
					}
				}
				assert( elems_ == count );
				delete[] list_;
				list_   = new_list;
				length_ = new_length;
			}
		};

		// A single shard of sharded cache.
		class lru_cache {
		public:
			lru_cache();
			~lru_cache();

			// Separate from constructor so caller can easily make an array of LRUcache
			void set_capacity( size_t capacity ) { capacity_ = capacity; }

			// Like cache methods, but with an extra "hash" parameter.
			cache::handle* insert( const slice& key, uint32_t hash, void* value, size_t charge,
														 core::function< void( const slice&, void* ) > deleter );
			cache::handle* look_up( const slice& key, uint32_t hash );
			void           release( cache::handle* handle );
			void           erase( const slice& key, uint32_t hash );
			void           prune();
			size_t         total_charge() const {
        MutexLock l( &mutex_ );
        return usage_;
			}

		private:
			void lru_remove( lru_handle* e );
			void lru_append( lru_handle* list, lru_handle* e );
			void ref( lru_handle* e );
			void un_ref( lru_handle* e );
			bool finish_erase( lru_handle* e );

			// Initialized before use.
			size_t capacity_;

			// mutex_ protects the following state.
			mutable port::mutex mutex_;
			size_t usage_       GUARDED_BY( mutex_ );

			// Dummy head of LRU list.
			// lru.prev is newest entry, lru.next is oldest entry.
			// Entries have refs==1 and in_cache==true.
			lru_handle lru_ GUARDED_BY( mutex_ );

			// Dummy head of in-use list.
			// Entries are in use by clients, and have refs >= 2 and in_cache==true.
			lru_handle in_use_ GUARDED_BY( mutex_ );

			handle_table table_ GUARDED_BY( mutex_ );
		};

		lru_cache::lru_cache()
				: capacity_( 0 )
				, usage_( 0 ) {
			// Make empty circular linked lists.
			lru_.next    = &lru_;
			lru_.prev    = &lru_;
			in_use_.next = &in_use_;
			in_use_.prev = &in_use_;
		}

		lru_cache::~lru_cache() {
			assert( in_use_.next == &in_use_ );// Error if caller has an unreleased handle
			for ( lru_handle* e = lru_.next; e != &lru_; ) {
				lru_handle* next = e->next;
				assert( e->in_cache );
				e->in_cache = false;
				assert( e->refs == 1 );// Invariant of lru_ list.
				un_ref( e );
				e = next;
			}
		}

		void lru_cache::ref( lru_handle* e ) {
			if ( e->refs == 1 && e->in_cache ) {// If on lru_ list, move to in_use_ list.
				lru_remove( e );
				lru_append( &in_use_, e );
			}
			e->refs++;
		}

		void lru_cache::un_ref( lru_handle* e ) {
			assert( e->refs > 0 );
			e->refs--;
			if ( e->refs == 0 ) {// Deallocate.
				assert( !e->in_cache );
				e->deleter( e->key(), e->value );
				free( e );
			} else if ( e->in_cache && e->refs == 1 ) {
				// No longer in use; move to lru_ list.
				lru_remove( e );
				lru_append( &lru_, e );
			}
		}

		void lru_cache::lru_remove( lru_handle* e ) {
			e->next->prev = e->prev;
			e->prev->next = e->next;
		}

		void lru_cache::lru_append( lru_handle* list, lru_handle* e ) {
			// Make "e" newest entry by inserting just before *list
			e->next       = list;
			e->prev       = list->prev;
			e->prev->next = e;
			e->next->prev = e;
		}

		cache::handle* lru_cache::look_up( const slice& key, uint32_t hash ) {
			MutexLock   l( &mutex_ );
			lru_handle* e = table_.look_up( key, hash );
			if ( e != nullptr ) {
				ref( e );
			}
			return reinterpret_cast< cache::handle* >( e );
		}

		void lru_cache::release( cache::handle* handle ) {
			MutexLock l( &mutex_ );
			un_ref( reinterpret_cast< lru_handle* >( handle ) );
		}

		cache::handle* lru_cache::insert( const slice& key, uint32_t hash, void* value, size_t charge,
																			core::function< void( const slice&, void* ) > deleter ) {
			MutexLock l( &mutex_ );

			lru_handle* e =
				reinterpret_cast< lru_handle* >( malloc( sizeof( lru_handle ) - 1 + key.size() ) );
			e->value      = value;
			e->deleter    = deleter;
			e->charge     = charge;
			e->key_length = key.size();
			e->hash       = hash;
			e->in_cache   = false;
			e->refs       = 1;// for the returned handle.
			std::memcpy( e->key_data, key.data(), key.size() );

			if ( capacity_ > 0 ) {
				e->refs++;// for the cache's reference.
				e->in_cache = true;
				lru_append( &in_use_, e );
				usage_ += charge;
				finish_erase( table_.insert( e ) );
			} else {// don't cache. (capacity_==0 is supported and turns off caching.)
				// next is read by key() in an assert, so it must be initialized
				e->next = nullptr;
			}
			while ( usage_ > capacity_ && lru_.next != &lru_ ) {
				lru_handle* old = lru_.next;
				assert( old->refs == 1 );
				bool erased = finish_erase( table_.remove( old->key(), old->hash ) );
				if ( !erased ) {// to avoid unused variable when compiled NDEBUG
					assert( erased );
				}
			}

			return reinterpret_cast< cache::handle* >( e );
		}

		// If e != nullptr, finish removing *e from the cache; it has already been
		// removed from the hash table.  Return whether e != nullptr.
		bool lru_cache::finish_erase( lru_handle* e ) {
			if ( e != nullptr ) {
				assert( e->in_cache );
				lru_remove( e );
				e->in_cache = false;
				usage_ -= e->charge;
				un_ref( e );
			}
			return e != nullptr;
		}

		void lru_cache::erase( const slice& key, uint32_t hash ) {
			MutexLock l( &mutex_ );
			finish_erase( table_.remove( key, hash ) );
		}

		void lru_cache::prune() {
			MutexLock l( &mutex_ );
			while ( lru_.next != &lru_ ) {
				lru_handle* e = lru_.next;
				assert( e->refs == 1 );
				bool erased = finish_erase( table_.remove( e->key(), e->hash ) );
				if ( !erased ) {// to avoid unused variable when compiled NDEBUG
					assert( erased );
				}
			}
		}

		static const int kNumShardBits = 4;
		static const int kNumShards    = 1 << kNumShardBits;

		class sharded_lru_cache : public cache {
		private:
			lru_cache   shard_[ kNumShards ];
			port::mutex id_mutex_;
			uint64_t    last_id_;

			static inline uint32_t Hashslice( const slice& s ) {
				return Hash( s.data(), s.size(), 0 );
			}

			static uint32_t Shard( uint32_t hash ) { return hash >> ( 32 - kNumShardBits ); }

		public:
			explicit sharded_lru_cache( size_t capacity )
					: last_id_( 0 ) {
				const size_t per_shard = ( capacity + ( kNumShards - 1 ) ) / kNumShards;
				for ( int s = 0; s < kNumShards; s++ ) {
					shard_[ s ].set_capacity( per_shard );
				}
			}
			~sharded_lru_cache() override {}
			handle* insert( const slice& key, void* value, size_t charge,
											core::function< void( const slice&, void* ) > deleter ) override {
				const uint32_t hash = Hashslice( key );
				return shard_[ Shard( hash ) ].insert( key, hash, value, charge, deleter );
			}
			handle* look_up( const slice& key ) override {
				const uint32_t hash = Hashslice( key );
				return shard_[ Shard( hash ) ].look_up( key, hash );
			}
			void release( handle* handle ) override {
				lru_handle* h = reinterpret_cast< lru_handle* >( handle );
				shard_[ Shard( h->hash ) ].release( handle );
			}
			void erase( const slice& key ) override {
				const uint32_t hash = Hashslice( key );
				shard_[ Shard( hash ) ].erase( key, hash );
			}
			void* value( handle* handle ) override {
				return reinterpret_cast< lru_handle* >( handle )->value;
			}
			uint64_t new_id() override {
				MutexLock l( &id_mutex_ );
				return ++( last_id_ );
			}
			void prune() override {
				for ( int s = 0; s < kNumShards; s++ ) {
					shard_[ s ].prune();
				}
			}
			size_t total_charge() const override {
				size_t total = 0;
				for ( int s = 0; s < kNumShards; s++ ) {
					total += shard_[ s ].total_charge();
				}
				return total;
			}
		};

	}// end anonymous namespace

	cache* new_lru_cache( size_t capacity ) { return new sharded_lru_cache( capacity ); }

}// namespace simple_leveldb
