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

static size_type random_height(size_type maxHeight);

constexpr size_type log2ceil(size_type value);

template <class PtrType>
std::uintptr_t to_packed_ptr(PtrType* ptr, std::uint32_t version, bool tag);

template <class PtrType>
PtrType* unpack_ptr(std::uintptr_t);

static constexpr std::uintptr_t Ptr_Mask = (std::uintptr_t(1) << 45) - std::uintptr_t(1);
static constexpr std::uintptr_t Tag_Mask = (std::uintptr_t(1) << 45);
static constexpr std::uintptr_t Version_Mask = ~(Ptr_Mask | Tag_Mask);
static constexpr std::uintptr_t Version_Step = Tag_Mask << 1;
}
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint = 64, class Allocator = std::allocator<std::uint8_t>, class Comparator = std::greater_equal<KeyType>>
class concurrent_priority_queue
{
	struct node
	{
		ValueType m_item;
		KeyType m_key;
		std::uint8_t m_height;

		std::atomic<std::uintptr_t> m_next[Max_Node_Height];
	};
public:
	using size_type = cpq_detail::size_type;

	static constexpr size_type Max_Node_Height = cpq_detail::log2ceil(ExpectedEntriesHint);
	
	using allocator_type = Allocator;
	using key_type = KeyType;
	using value_type = ValueType;
	using comparator_type = Comparator;
	using node_type = node;

	concurrent_priority_queue();

	void insert(node_type* item);

	bool try_pop(node_type*& out);
private:
	bool try_insert(node_type* item);
	void find_predecessors_successors(node_type* outPredecessor[Max_Node_Height], node_type* outSuccessor[Max_Node_Height], const key_type& key);

	node_type m_head;
	node_type m_end;

	comparator_type m_comparator;
};
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::concurrent_priority_queue()
	: m_head()
	, m_end()
	, m_comparator()
{
	m_head.m_key = std::numeric_limits<key_type>::min();
	m_end.m_key = std::numeric_limits<key_type>::max();
	m_head.m_height = Max_Node_Height;

	for (std::size_t i = 0; i < Max_Node_Height; ++i) {
		m_head.m_next[i].store(jh_detail::to_packed_ptr(&m_end, 0, false), std::memory_order_relaxed);
	}
}

template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::insert(node_type* item)
{
	// Generate height... &&... Refs? (try to avoid)
	while (!try_insert(item));
}

template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::try_pop(node_type*& out)
{
	return false;
}
template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::try_insert(node_type* item)
{
	node* predecessors[Max_Node_Height];
	node* successors[Max_Node_Height];

	find_predecessors_successors(predecessors, successors, item->m_key);

	for (std::uint8_t i = 0; i < item->m_height; ++i) {
		item->m_next[i].store(successors[i], std::memory_order_relaxed);
	}


	return false;
}
template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline void concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::find_predecessors_successors(node_type* outPredecessor[Max_Node_Height], node_type* outSuccessor[Max_Node_Height], const key_type& key)
{
	for (std::uint8_t i = 0; i < Max_Node_Height; ++i) {
		const std::uint8_t invertedIndex(Max_Node_Height - i - 1);

		node* current(&m_head);
		for (;;) {
			const std::uintptr_t nextValue(current->m_next[invertedIndex].load(std::memory_order_relaxed));
			node* const nextPtr(cpq_detail::unpack_ptr(nextValue));

			if (!m_comparator(key, next->m_key) ||
				next == &m_end) {
				outSuccessor[invertedIndex] = nextPtr;
				break;
			}
			current = nextPtr;
		}
		outPredecessor[invertedIndex] = current;
	}
}

namespace cpq_detail
{
static size_type random_height(size_type maxHeight)
{
	struct tl_container
	{
		// https://codingforspeed.com/using-faster-psudo-random-generator-xorshift/

		std::uint32_t x = 123456789;
		std::uint32_t y = 362436069;
		std::uint32_t z = 521288629;
		std::uint32_t w = 88675123;

		std::uint32_t get() 
		{
			std::uint32_t t;
			t = x ^ (x << 11);
			x = y; y = z; z = w;
			return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
		}
	//

	}static thread_local t_rng;

	const size_type lowerBound(2);
	const size_type upperBound(static_cast<size_type>(std::pow(2, maxHeight)));
	const size_type range((upperBound + 1) - lowerBound);
	const size_type random(lowerBound + (t_rng.get() % range));
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

template <class PtrType>
std::uintptr_t to_packed_ptr(PtrType* ptr, std::uint32_t version, bool tag) 
{
	const std::uintptr_t ptrValue((std::uintptr_t)ptr);
	const std::uintptr_t tagValue(tag);
	const std::uintptr_t versionValue(version);

	const std::uintptr_t shiftedPtrValue(ptrValue >> 3);
	const std::uintptr_t shiftedTagValue(tagValue << 45);
	const std::uintptr_t shiftedVersionValue(versionValue << 46);

	return (shiftedPtrValue | shiftedTagValue | shiftedVersionValue);
}
template <class PtrType>
PtrType* unpack_ptr(std::uintptr_t packedPtr) 
{
	return (packedPtr & Ptr_Mask) << 3;
}
}
}