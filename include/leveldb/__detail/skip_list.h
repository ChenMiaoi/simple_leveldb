// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_SIMPLE_LEVELDB_DB_SKIPLIST_H
#define STORAGE_SIMPLE_LEVELDB_DB_SKIPLIST_H

// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...

#include <atomic>
#include <cassert>
#include <cstdlib>

#include "util/arena.h"
#include "util/random.h"

namespace simple_leveldb {

	template < typename Key, class Comparator >
	class skip_list {
	private:
		struct node;

	public:
		// Create a new SkipList object that will use "cmp" for comparing keys,
		// and will allocate memory using "*arena".  Objects allocated in the arena
		// must remain allocated for the lifetime of the skiplist object.
		explicit skip_list( Comparator cmp, arena* arena );

		skip_list( const skip_list& )            = delete;
		skip_list& operator=( const skip_list& ) = delete;

		// Insert key into the list.
		// REQUIRES: nothing that compares equal to key is currently in the list.
		void insert( const Key& key );

		// Returns true iff an entry that compares equal to key is in the list.
		bool contains( const Key& key ) const;

		// Iteration over the contents of a skip list
		class iterator {
		public:
			// Initialize an iterator over the specified list.
			// The returned iterator is not valid.
			explicit iterator( const skip_list* list );

			// Returns true iff the iterator is positioned at a valid node.
			bool valid() const;

			// Returns the key at the current position.
			// REQUIRES: Valid()
			const Key& key() const;

			// Advances to the next position.
			// REQUIRES: Valid()
			void next();

			// Advances to the previous position.
			// REQUIRES: Valid()
			void prev();

			// Advance to the first entry with a key >= target
			void seek( const Key& target );

			// Position at the first entry in list.
			// Final state of iterator is Valid() iff list is not empty.
			void seek_to_first();

			// Position at the last entry in list.
			// Final state of iterator is Valid() iff list is not empty.
			void seek_to_last();

		private:
			const skip_list* list_;
			node*            node_;
			// Intentionally copyable
		};

	private:
		enum { kMaxHeight = 12 };

		inline int get_max_height() const {
			return max_height_.load( core::memory_order_relaxed );
		}

		node* new_node( const Key& key, int height );
		int   random_height();
		bool  equal( const Key& a, const Key& b ) const { return ( compare_( a, b ) == 0 ); }

		// Return true if key is greater than the data stored in "n"
		bool key_is_after_node( const Key& key, node* n ) const;

		// Return the earliest node that comes at or after key.
		// Return nullptr if there is no such node.
		//
		// If prev is non-null, fills prev[level] with pointer to previous
		// node at "level" for every level in [0..max_height_-1].
		node* find_greater_or_equal( const Key& key, node** prev ) const;

		// Return the latest node with a key < key.
		// Return head_ if there is no such node.
		node* find_less_than( const Key& key ) const;

		// Return the last node in the list.
		// Return head_ if list is empty.
		node* find_last() const;

		// Immutable after construction
		Comparator const compare_;
		arena* const     arena_;// arena used for allocations of nodes

		node* const head_;

		// Modified only by Insert().  Read racily by readers, but stale
		// values are ok.
		core::atomic< int > max_height_;// Height of the entire list

		// Read/written only by Insert().
		random rnd_;
	};

	// Implementation details follow
	template < typename Key, class Comparator >
	struct skip_list< Key, Comparator >::node {
		explicit node( const Key& k )
				: key( k ) {}

		Key const key;

		// Accessors/mutators for links.  Wrapped in methods so we can
		// add the appropriate barriers as necessary.
		node* next( int n ) {
			assert( n >= 0 );
			// Use an 'acquire load' so that we observe a fully initialized
			// version of the returned Node.
			return next_[ n ].load( core::memory_order_acquire );
		}
		void set_next( int n, node* x ) {
			assert( n >= 0 );
			// Use a 'release store' so that anybody who reads through this
			// pointer observes a fully initialized version of the inserted node.
			next_[ n ].store( x, core::memory_order_release );
		}

		// No-barrier variants that can be safely used in a few locations.
		node* no_barrier_next( int n ) {
			assert( n >= 0 );
			return next_[ n ].load( core::memory_order_relaxed );
		}
		void no_barrier_set_next( int n, node* x ) {
			assert( n >= 0 );
			next_[ n ].store( x, core::memory_order_relaxed );
		}

	private:
		// Array of length equal to the node height.  next_[0] is lowest level link.
		core::atomic< node* > next_[ 1 ];
	};

	template < typename Key, class Comparator >
	typename skip_list< Key, Comparator >::node* skip_list< Key, Comparator >::new_node(
		const Key& key, int height ) {
		// 由于内存一致性，这里分配的空间实际上是同时给head_和head_.next_分配空间
		// sizeof(Node)给head_本身分配了空间，也就是包括head_.key和head_.next_[1]
		// sizeof(core::atomic<Node*>) * (height - 1)给head_.next_的额外层级分配了空间，因此在后续才能直接的访问
		char* const node_memory = arena_->allocate_aligned(
			sizeof( node ) + sizeof( core::atomic< node* > ) * ( height - 1 ) );
		return new ( node_memory ) node( key );
	}

	template < typename Key, class Comparator >
	inline skip_list< Key, Comparator >::iterator::iterator( const skip_list* list ) {
		list_ = list;
		node_ = nullptr;
	}

	template < typename Key, class Comparator >
	inline bool skip_list< Key, Comparator >::iterator::valid() const {
		return node_ != nullptr;
	}

	template < typename Key, class Comparator >
	inline const Key& skip_list< Key, Comparator >::iterator::key() const {
		assert( valid() );
		return node_->key;
	}

	template < typename Key, class Comparator >
	inline void skip_list< Key, Comparator >::iterator::next() {
		assert( valid() );
		node_ = node_->next( 0 );
	}

	template < typename Key, class Comparator >
	inline void skip_list< Key, Comparator >::iterator::prev() {
		// Instead of using explicit "prev" links, we just search for the
		// last node that falls before key.
		assert( valid() );
		node_ = list_->find_less_than( node_->key );
		if ( node_ == list_->head_ ) {
			node_ = nullptr;
		}
	}

	template < typename Key, class Comparator >
	inline void skip_list< Key, Comparator >::iterator::seek( const Key& target ) {
		node_ = list_->find_greater_or_equal( target, nullptr );
	}

	template < typename Key, class Comparator >
	inline void skip_list< Key, Comparator >::iterator::seek_to_first() {
		node_ = list_->head_->Next( 0 );
	}

	template < typename Key, class Comparator >
	inline void skip_list< Key, Comparator >::iterator::seek_to_last() {
		node_ = list_->find_last();
		if ( node_ == list_->head_ ) {
			node_ = nullptr;
		}
	}

	template < typename Key, class Comparator >
	int skip_list< Key, Comparator >::random_height() {
		// Increase height with probability 1 in kBranching
		static const unsigned int kBranching = 4;
		int                       height     = 1;
		while ( height < kMaxHeight && rnd_.one_in( kBranching ) ) {
			height++;
		}
		assert( height > 0 );
		assert( height <= kMaxHeight );
		return height;
	}

	template < typename Key, class Comparator >
	bool skip_list< Key, Comparator >::key_is_after_node( const Key& key, node* n ) const {
		// null n is considered infinite
		return ( n != nullptr ) && ( compare_( n->key, key ) < 0 );
	}

	template < typename Key, class Comparator >
	typename skip_list< Key, Comparator >::node*
	skip_list< Key, Comparator >::find_greater_or_equal( const Key& key,
																											 node**     prev ) const {
		node* x     = head_;
		int   level = get_max_height() - 1;
		while ( true ) {
			node* next = x->next( level );
			if ( key_is_after_node( key, next ) ) {// 如果key还在node之后，就还需要往后查找
				// Keep searching in this list
				x = next;
			} else {
				if ( prev != nullptr ) prev[ level ] = x;// 记录下每一层的最后一个节点
				if ( level == 0 ) {
					return next;
				} else {
					// Switch to next list
					level--;
				}
			}
		}
	}

	template < typename Key, class Comparator >
	typename skip_list< Key, Comparator >::node*
	skip_list< Key, Comparator >::find_less_than( const Key& key ) const {
		node* x     = head_;
		int   level = get_max_height() - 1;
		while ( true ) {
			assert( x == head_ || compare_( x->key, key ) < 0 );
			node* next = x->next( level );
			if ( next == nullptr || compare_( next->key, key ) >= 0 ) {
				if ( level == 0 ) {
					return x;
				} else {
					// Switch to next list
					level--;
				}
			} else {
				x = next;
			}
		}
	}

	template < typename Key, class Comparator >
	typename skip_list< Key, Comparator >::node* skip_list< Key, Comparator >::find_last()
		const {
		node* x     = head_;
		int   level = get_max_height() - 1;
		while ( true ) {
			node* next = x->next( level );
			if ( next == nullptr ) {
				if ( level == 0 ) {
					return x;
				} else {
					// Switch to next list
					level--;
				}
			} else {
				x = next;
			}
		}
	}

	template < typename Key, class Comparator >
	skip_list< Key, Comparator >::skip_list( Comparator cmp, arena* arena )
			: compare_( cmp )
			, arena_( arena )
			, head_( new_node( 0 /* any key will do */, kMaxHeight ) )
			, max_height_( 1 )
			, rnd_( 0xdeadbeef ) {

		// 尽管head_.next_[1]定义时看起来只有一个元素，但是在NewNode中，我们开辟了所有高度的头节点的空间
		for ( int i = 0; i < kMaxHeight; i++ ) {
			head_->set_next( i, nullptr );
		}
	}

	template < typename Key, class Comparator >
	void skip_list< Key, Comparator >::insert( const Key& key ) {
		// TODO(opt): We can use a barrier-free variant of FindGreaterOrEqual()
		// here since Insert() is externally synchronized.
		node* prev[ kMaxHeight ];
		node* x = find_greater_or_equal( key, prev );

		// Our data structure does not allow duplicate insertion
		assert( x == nullptr || !equal( key, x->key ) );

		int height = random_height();
		if ( height > get_max_height() ) {
			for ( int i = get_max_height(); i < height; i++ ) {
				prev[ i ] = head_;
			}
			// It is ok to mutate max_height_ without any synchronization
			// with concurrent readers.  A concurrent reader that observes
			// the new value of max_height_ will see either the old value of
			// new level pointers from head_ (nullptr), or a new value set in
			// the loop below.  In the former case the reader will
			// immediately drop to the next level since nullptr sorts after all
			// keys.  In the latter case the reader will use the new node.
			max_height_.store( height, core::memory_order_relaxed );
		}

		x = new_node( key, height );
		for ( int i = 0; i < height; i++ ) {
			// NoBarrier_SetNext() suffices since we will add a barrier when
			// we publish a pointer to "x" in prev[i].
			// x的第i层指向prev的第i层的下一个节点
			// 然后prev的第i层的前驱节点指向x
			x->no_barrier_set_next( i, prev[ i ]->no_barrier_next( i ) );
			prev[ i ]->set_next( i, x );
		}
	}

	template < typename Key, class Comparator >
	bool skip_list< Key, Comparator >::contains( const Key& key ) const {
		node* x = find_greater_or_equal( key, nullptr );
		if ( x != nullptr && equal( key, x->key ) ) {
			return true;
		} else {
			return false;
		}
	}

}// namespace simple_leveldb

#endif//! STORAGE_SIMPLE_LEVELDB_DB_SKIPLIST_H
