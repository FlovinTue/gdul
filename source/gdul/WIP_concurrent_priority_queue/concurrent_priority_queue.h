// Copyright(c) 2020 Flovin Michaelsen
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
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/pool_allocator/pool_allocator.h>
#include <random>

#ifndef MAKE_UNIQUE_NAME 
#define CONCAT(a,b)  a##b
#define EXPAND_AND_CONCAT(a, b) CONCAT(a,b)
#define MAKE_UNIQUE_NAME(prefix) EXPAND_AND_CONCAT(prefix, __COUNTER__)
#endif

#define GDUL_CPQ_PADDING(bytes) const std::uint8_t MAKE_UNIQUE_NAME(padding)[bytes] {}

#pragma warning(disable:4505)

namespace gdul
{
namespace cpq_detail
{
using size_type = std::size_t;
	
struct random;

static size_type random_height(std::mt19937& rng, size_type maxHeight);

constexpr size_type log2ceil(size_type value);


}
/// <summary>
/// ExpectedEntriesHint should be somewhere between the average and the maximum number of entries
/// contained within the queue at any given time. It is used to calculate the maximum node height 
/// in the skip-list.
/// </summary>
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint = 64, class Allocator = std::allocator<std::uint8_t>, class Comparator = std::greater_equal<KeyType>>
class concurrent_priority_queue
{
public:
	using allocator_type = Allocator;
	using key_type = KeyType;
	using value_type = ValueType;
	using size_type = cpq_detail::size_type;
	using comparator_type = Comparator;

	concurrent_priority_queue();

	/// <summary>
	/// Insert an item in to the queue
	/// </summary>
	/// <param name="item">Key value pair to insert</param>
	void insert(const std::pair<key_type, value_type>& item);
	
	/// <summary>
	/// Attempt to retrieve the first item
	/// </summary>
	/// <param name="out">The out value item</param>
	/// <returns>Indicates if the operation was successful</returns>
	bool try_pop(value_type& out);
	static constexpr size_type Max_Node_Height = cpq_detail::log2ceil(ExpectedEntriesHint);
private:

	struct node
	{
		node()
			: m_next{}
			, m_kvPair()
			, m_height(0)
		{}
		atomic_shared_ptr<node> m_next[Max_Node_Height];

		std::pair<key_type, value_type> m_kvPair;

		std::uint8_t m_height;
	};

	memory_pool m_nodeMemPool;

	node m_head;
	node m_end;

	comparator_type m_comparator;
};
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::concurrent_priority_queue() 
	: m_nodeMemPool()
	, m_head()
	, m_end()
	, m_comparator()
{
	m_nodeMemPool.init<gdul::allocate_shared_size<node, allocator_type>(), alignof(node)>(128);

	for (size_type i = 0; i < Max_Node_Height; ++i) {
		m_head.m_next[i].store(&m_end, std::memory_order_relaxed);
	}
}
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::insert(const std::pair<key_type, value_type>& item)
{
	// Searching begins at top and whittles down to the bottom. 
	// Finding the desired value is done at the bottom level *always*
	// and the upper levels are only there to speed up the search
	// process.
	// Values may be inserted only at the bottom and 
	// they will still be found
}
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::try_pop(value_type& out) 
{
	if (m_head.m_next[0].load(std::memory_order_relaxed) == &m_end)
		return false;


	// What about de-linking?
	// It must happen from bottom-up

	// I can't imagine this being very efficient with not-so-many jobs.
	// Interested in finding a more effeicient way of inserting. Or... Uh 
	// Another structure... Uhm. Dang skip list. So unelegant. 

	// Want insertions to... Umm have a predictable cost.
	// Something resembling this idea would have like 1 atomic fetch add
	// each skip. Then the linking step would have to compare exchange
	// avg 3 atomic shared pointers to refer to the new node. 

	// If we use regular atomics. Then those fetch_adds would be loads
	// A search of 64 items to the middle would have like 6 loads
	// and 3 cas *optimally*. With chance for being interrupted....
	// 

	// ... The last part is the worst. Those cas ops are just the worst.
	// Want to be able to link with ONE cas. Or store.

	// What about some kind of array with linking blocks?

	// Any one node will always have only *one* configuration
	// of linking block.

	// popping is fine. Good. If the head node has 1 height a pop attempt
	// should only cost 1 cas. and 1 load.

	// Maybe a heap... Umm.. With 


	// ^

	// Want to (still) implement a concurrent_heap. Perhaps fine grained locking
	// or lock free..
	// 


}
namespace cpq_detail
{
struct random
{
	static std::random_device s_rd;
	static thread_local std::mt19937 t_rng;
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
std::random_device random::s_rd;
thread_local std::mt19937 random::t_rng(random::s_rd());
}
}