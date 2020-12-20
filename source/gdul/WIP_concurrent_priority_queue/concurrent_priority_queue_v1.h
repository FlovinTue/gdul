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
#include <gdul/memory/pool_allocator.h>
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
/// contained within the queue at any given time. It is used to calculate the maximum cpq_node height 
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

	static constexpr size_type Max_Node_Height = cpq_detail::log2ceil(ExpectedEntriesHint);

	class cpq_node
	{
	public:
		cpq_node(value_type& item)
			: m_nextHint{}
			, m_item(&item)
			, m_key()
			, m_height(0)
			, m_next(nullptr)
		{}

		value_type& item() {
			return m_item;
		}
		const value_type& item() const {
			return m_item;
		}

	private:
		friend class concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>;

		std::uintptr_t to_versioned_ptr(cpq_node* ptr, std::uint16_t version) {
			const std::uintptr_t ptrValue((std::uintptr_t)ptr);
			const std::uintptr_t versionValue((std::uintptr_t)version);
			const std::uintptr_t shiftedVersionValue(versionValue << 48);
			return (ptrValue | shiftedVersionValue);
		}
		void fetch_next_ptr(cpq_node&* outPtr, std::uint16_t& outVersion) {
			const std::uintptr_t value(m_next.load(std::memory_order_relaxed));
			const std::uintptr_t ptrValue((value << 16) >> 16);
			const std::uintptr_t versionValue(value >> 48);
			outPtr = (cpq_node*)ptrValue;
			outVersion = (std::uint16_t)versionValue;
		}
		bool try_exchange_next(cpq_node* expectedPtr, std::uint16_t expectedVersion, cpq_node* desiredPtr) {

			const std::uintptr_t desired(to_versioned_ptr(desiredPtr, expectedVersion + 1));
			std::uintptr_t expected(to_versioned_ptr(expectedPtr, expectedVersion));
			return m_next.compare_exchange_strong(expected, desired, std::memory_order_acquire, std::memory_order_relaxed);
		}

		cpq_node* m_nextHint[Max_Node_Height];

		value_type& m_item;

		key_type m_key;
		std::uint8_t m_height;

		std::atomic<std::uintptr_t> m_next;
	};

	concurrent_priority_queue();

	/// <summary>
	/// Insert an item in to the queue
	/// </summary>
	/// <param name="item">Key value pair to insert</param>
	void insert(cpq_node* toInsert);
	
	/// <summary>
	/// Attempt to retrieve the first item
	/// </summary>
	/// <param name="out">The out value item</param>
	/// <returns>Indicates if the operation was successful</returns>
	bool try_pop(cpq_node*& out);
private:

	static constexpr std::uintptr_t Ptr_Mask = ~(std::uintptr_t(std::numeric_limits<std::uint16_t>::max()) << 48);

	std::atomic<uintptr_t> m_head;

	comparator_type m_comparator;
};
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::concurrent_priority_queue() 
	: m_head()
	, m_comparator()
{}

template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::try_pop(cpq_node * &out)
{
	std::uintptr_t head(0);

	do {
		head = m_head.load(std::memory_order_relaxed);

		cpq_node* const next((cpq_node*)head & Ptr_Mask);
		const std::uint16_t version((std::uint16_t)head >> 48);


	} while (head & Ptr_Mask);
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