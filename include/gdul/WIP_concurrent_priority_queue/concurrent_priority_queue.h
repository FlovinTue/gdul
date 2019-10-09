// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <atomic>
#include <vector>
#include <iostream>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <random>
#include <sstream>

#ifndef MAKE_UNIQUE_NAME 
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __COUNTER__)
#endif

#define CSL_PADD(bytes) const uint8_t MAKE_UNIQUE_NAME(padding)[bytes] {}

#pragma warning(disable:4505)

namespace gdul
{
namespace cpqdetail
{
typedef std::size_t size_type;
	
struct random;

static size_type random_height(std::mt19937& rng, size_type maxHeight);

constexpr size_type log2ceil(size_type value);

template <class KeyType, class ValueType, size_type MaxNodeHeight, class Allocator>
class node;

struct tiny_less;

}
// ExpectedEntriesHint should be somewhere between the average and the maximum number of entries
// contained within the queue at any given time. It is used to calculate the maximum node height 
// in the skip-list.
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator = cpqdetail::tiny_less>
class concurrent_priority_queue
{
private:
	template <class Dummy>
	class allocator;


public:
	typedef cpqdetail::size_type size_type;
	
	static constexpr size_type Max_Node_Height = cpqdetail::log2ceil(ExpectedEntriesHint);

	typedef Comparator comparator_type;
	typedef KeyType key_type;
	typedef ValueType value_type;
	typedef allocator<uint8_t> allocator_type;
	typedef cpqdetail::node<key_type, value_type, Max_Node_Height, allocator_type> node_type;
	typedef shared_ptr<node_type, allocator_type> shared_ptr_type;
	typedef atomic_shared_ptr<node_type, allocator_type> atomic_shared_ptr_type;
	typedef versioned_raw_ptr<node_type, allocator_type> versioned_raw_ptr_type;

	concurrent_priority_queue();
	~concurrent_priority_queue();

	size_type size() const;

	void insert(const std::pair<key_type, value_type>& in);
	void insert(std::pair<key_type, value_type>&& in);

	bool try_pop(value_type& out);
	bool try_pop(std::pair<key_type, value_type>& out);

	// Compares the value of out.first to that of top key, if they match
	// a pop is attempted. Changes out.first to existing value on faliure
	bool compare_try_pop(std::pair<key_type, value_type>& out);

	// Top key hint
	bool try_peek_top_key(key_type& out);

	void unsafe_clear();

private:

	struct alloc_size_rep
	{
		atomic_shared_ptr_type myNext[Max_Node_Height];
		std::pair<key_type, value_type> myKeyValuePair;
		const size_type myHeight;
	};
	class alloc_type
	{
		uint8_t myBlock[shared_ptr<alloc_size_rep>::Alloc_Size_Make_Shared];
	};

	template <class Dummy>
	class allocator
	{
	public:
		allocator(concurrent_object_pool<alloc_type>* memPool) : myMemoryPool(memPool) {}
		allocator(const allocator<uint8_t>& other) : myMemoryPool(other.myMemoryPool) {}

		typedef uint8_t value_type;

		uint8_t* allocate(std::size_t /*n*/) {
			return reinterpret_cast<uint8_t*>(myMemoryPool->get_object());
		}
		void deallocate(uint8_t* ptr, std::size_t /*n*/) {
			myMemoryPool->recycle_object(reinterpret_cast<alloc_type*>(ptr));
		}

	private:
		concurrent_object_pool<alloc_type>* myMemoryPool;
	};

	bool try_insert(shared_ptr_type& entry);
	bool try_pop_internal(key_type& expectedKey, value_type& outValue, bool matchKey);
	bool search(node_type* (&insertionPoint)[Max_Node_Height], shared_ptr_type (&last) [Max_Node_Height], shared_ptr_type(&current)[Max_Node_Height], const key_type& key, size_type atHeight);

	void try_splice(node_type* at, shared_ptr_type&& with, shared_ptr_type& current);
	void try_link_if_unlinked(shared_ptr_type & toLink, node_type *(&last)[Max_Node_Height], shared_ptr_type(&expected)[Max_Node_Height]);

	std::atomic<size_type> mySize;

	CSL_PADD(64 - (sizeof(mySize) % 64));
	concurrent_object_pool<alloc_type> myMemoryPool;
	allocator_type myAllocator;
	CSL_PADD(64 - ((sizeof(myMemoryPool) + sizeof(myAllocator)) % 64));
	comparator_type myComparator;
	shared_ptr_type myFrontSentry;
	shared_ptr_type myEndSentry;
};
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint, class Comparator>
inline concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::concurrent_priority_queue()
	: mySize(0)
	, myMemoryPool(128)
	, myAllocator(&myMemoryPool)
	, myFrontSentry(make_shared<node_type, allocator_type>(myAllocator, Max_Node_Height))
	, myEndSentry(make_shared<node_type, allocator_type>(myAllocator, Max_Node_Height))
{
	static_assert(std::is_integral<key_type>::value || std::is_floating_point<key_type>::value, "Only integers and floats allowed as key type");

	for (size_type i = 0; i < Max_Node_Height; ++i){
		myFrontSentry->myNext[i].unsafe_store(myEndSentry);
	}
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::~concurrent_priority_queue()
{
	unsafe_clear();
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline typename concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::size_type concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::size() const
{
	return mySize.load(std::memory_order_acquire);
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::insert(const std::pair<key_type, value_type>& in)
{
	insert(std::pair<key_type, value_type>(in));
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::insert(std::pair<key_type, value_type>&& in)
{
	//shared_ptr_type entry(make_shared<node_type, allocator_type>(myAllocator, cpqdetail::random_height(cpqdetail::random::ourRng, Max_Node_Height)));
	shared_ptr_type entry(make_shared<node_type, allocator_type>(myAllocator, 5));
	entry->myKeyValuePair = std::move(in);

	while (!try_insert(entry));

	mySize.fetch_add(1, std::memory_order_relaxed);
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_pop(value_type & out)
{
	key_type dummy(0);
	return try_pop_internal(dummy, out, false);
}

template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_pop(std::pair<key_type, value_type>& out)
{
	return try_pop_internal(out.first, out.second, false);
}

template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::compare_try_pop(std::pair<key_type, value_type>& out)
{
	return try_pop_internal(out.first, out.second, true);
}

template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_peek_top_key(key_type & out)
{
	const shared_ptr_type head(myFrontSentry->myNext[0].load());

	if (!head) {
		return false;
	}

	out = head->myKeyValuePair.first;

	return true;
}

template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::unsafe_clear()
{
	std::vector<node_type*> arr;
	arr.reserve(mySize.load(std::memory_order_acquire));

	node_type* prev(static_cast<node_type*>(myFrontSentry));

	for (size_type i = 0; i < mySize.load(std::memory_order_relaxed); ++i) {
		prev = static_cast<node_type*>(prev->myNext[0]);
		arr.push_back(prev);
	}
	for (typename std::vector<node_type*>::reverse_iterator it = arr.rbegin(); it != arr.rend(); ++it) {
		(*it)->myNext[0].unsafe_store(nullptr);
	}
	mySize.store(0, std::memory_order_relaxed);
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_insert(shared_ptr_type& entry)
{
	// Nodes will always be encountered first at their topmost level.... deletes need to begin there..

	// Insertion points will always be found at the bottom most layer. Insertions need to happen there..
	// Find bottom node
	// .... And find all next nodes for current height? 
	// ... And all previous insertionpoints for current height?

	shared_ptr_type last[Max_Node_Height]{ nullptr };
	node_type* insertionPoint[Max_Node_Height]{ nullptr };
	std::fill(&insertionPoint[0], &insertionPoint[Max_Node_Height], static_cast<node_type*>(myFrontSentry));

	shared_ptr_type current[Max_Node_Height]{ nullptr };

	for (size_type i = 0; i < Max_Node_Height; ++i) {
		const size_type inverseIndex((Max_Node_Height - 1) - i);
	
		current[inverseIndex] = insertionPoint[inverseIndex]->myNext[inverseIndex].load();
		if (!search(insertionPoint, last, current, entry->myKeyValuePair.first, inverseIndex)) {
			return false;
		}
	}
	
	versioned_raw_ptr_type expected(current[0].get_versioned_raw_ptr());
	entry->myNext[0].unsafe_store(std::move(current[0]));

	if (insertionPoint[0]->myNext[0].compare_exchange_strong(expected, entry)) {
		try_link_if_unlinked(entry, insertionPoint, current);
		return true;
	}

	return false;
}

template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_pop_internal(key_type & expectedKey, value_type & outValue, bool matchKey)
{
	const size_type currentSize(mySize.fetch_sub(1, std::memory_order_acq_rel) - 1);
	const size_type difference(std::numeric_limits<size_type>::max() - (currentSize));
	const size_type threshhold(std::numeric_limits<size_type>::max() / 2);

	if (difference < threshhold) {
		mySize.fetch_add(1, std::memory_order_relaxed);
		return false;
	}

	shared_ptr_type head(nullptr);
	shared_ptr_type splice(nullptr);

	versioned_raw_ptr_type expected(nullptr);

	for (;;) {
		head = myFrontSentry->myNext[0].load();

		const key_type key(head->myKeyValuePair.first);
		if (matchKey & (expectedKey != key)) {
			expectedKey = key;
			return false;
		}

		splice = head->myNext[0].load_and_tag();
		bool mine(!splice.get_tag());
		splice.clear_tag();

		try_splice(myFrontSentry.get_owned(), std::move(splice), head);

		if (mine) {
			break;
		}
	}
	expectedKey = head->myKeyValuePair.first;
	outValue = head->myKeyValuePair.second;

	return true;
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::search(node_type* (&insertionPoint)[Max_Node_Height], shared_ptr_type(&last)[Max_Node_Height], shared_ptr_type(&current)[Max_Node_Height], const key_type& key, size_type atHeight)
{
	while (current[atHeight] != myEndSentry) {
		try_link_if_unlinked(current[atHeight], insertionPoint, current);

		if (myComparator(key, current[atHeight]->myKeyValuePair.first)) {
			break;
		}

		shared_ptr_type next(current[atHeight]->myNext[atHeight].load());

		if (next.get_tag()) {
			next.clear_tag();

			try_splice(insertionPoint[atHeight], std::move(next), current[atHeight]);

			current[atHeight] = insertionPoint[atHeight]->myNext[atHeight].load();

			if (current[atHeight].get_tag()) {
				return false;
			}
		}
		else {
			last[atHeight] = std::move(current[atHeight]);
			current[atHeight] = std::move(next);
			insertionPoint[atHeight] = static_cast<node_type*>(last[atHeight]);

		}
	};
	return true;
}
template <class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint , class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_splice(node_type* at, shared_ptr_type&& with, shared_ptr_type& current)
{
	versioned_raw_ptr_type expected(current);
	if (at->myNext[0].compare_exchange_strong(expected, std::move(with))) {
		shared_ptr_type null(nullptr);
		null.set_tag();
		current->myNext[0].store(null);
	}
}
template<class KeyType, class ValueType, cpqdetail::size_type ExpectedEntriesHint, class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Comparator>::try_link_if_unlinked(shared_ptr_type & toLink, node_type *(&last)[Max_Node_Height], shared_ptr_type(&expected)[Max_Node_Height])
{
	// Must combine a write to current->myNext[index] with a successfull previous->current linkage....

	const size_type height(toLink->myHeight);
	versioned_raw_ptr_type expected_[Max_Node_Height]{ nullptr };
	memcpy(&expected_[0], &expected[0], sizeof(shared_ptr_type) * Max_Node_Height);

	for (size_type index(toLink->myNumLinked.load(std::memory_order_acquire));  index < height; index = (toLink->myNumLinked.fetch_add(1, std::memory_order_acq_rel) + 1)){
		if (!last[index]->myNext[index].compare_exchange_strong(expected_[index], toLink)){
			toLink->myNumLinked.fetch_sub(1, std::memory_order_relaxed);
			break;
		}
	}
}
namespace cpqdetail
{
struct random
{
	static thread_local std::random_device ourRd;
	static thread_local std::mt19937 ourRng;
};
template <class KeyType, class ValueType, size_type MaxNodeHeight, class Allocator>
class node
{
public:
	typedef KeyType key_type;
	typedef ValueType value_type;
	typedef Allocator allocator_type;
	typedef typename concurrent_priority_queue<key_type, value_type, 2, tiny_less>::size_type size_type;
	typedef atomic_shared_ptr<node<key_type, value_type, MaxNodeHeight, allocator_type>, Allocator> atomic_shared_ptr_type;

	constexpr node(size_type height);

	std::pair<key_type, value_type> myKeyValuePair;
	atomic_shared_ptr_type myNext[MaxNodeHeight];
	std::atomic<size_type> myNumLinked;
	const size_type myHeight;
};
template <class KeyType, class ValueType, size_type MaxNodeHeight, class Allocator>
inline constexpr node<KeyType, ValueType, MaxNodeHeight, Allocator>::node(size_type height)
	: myKeyValuePair{ std::numeric_limits<key_type>::max(), value_type() }
	, myNext{ nullptr }
	, myNumLinked(1)
	, myHeight(height)
{

}
struct tiny_less
{
	template <class T>
	constexpr bool operator()(const T& a, const T& b) const
	{
		return a < b;
	};
};
static size_type random_height(std::mt19937& rng, size_type maxHeight)
{
	const size_type lowerBound(2);
	const size_type upperBound(static_cast<size_type>(std::pow(2, maxHeight)));
	const std::uniform_int_distribution<size_type> dist(lowerBound, upperBound);
	const size_type random(dist(rng));
	const size_type height(log2ceil(random));
	const size_type shiftedHeight(height - 1);

	const size_type invShiftedHeight(maxHeight - shiftedHeight);

	return invShiftedHeight;
}
constexpr size_type log2ceil(size_type value) {
	size_type highBit(0);
	size_type shift(value);
	for (; shift; ++highBit) {
		shift >>= 1;
	}

	highBit -= static_cast<bool>(highBit);

	const size_type mask((size_type(1) << (highBit)) - 1);
	const size_type remainder((static_cast<bool>(value & mask)));

	return highBit + remainder;
}
thread_local std::random_device random::ourRd;
thread_local std::mt19937 random::ourRng(random::ourRd());
}
}