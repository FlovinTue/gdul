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

std::uintptr_t to_versioned_ptr(void* ptr, std::uint16_t version) {
	const std::uintptr_t ptrValue((std::uintptr_t)ptr);
	const std::uintptr_t versionValue((std::uintptr_t)version);
	const std::uintptr_t shiftedVersionValue(versionValue << 48);
	return (ptrValue | shiftedVersionValue);
}

template <class Ptr>
void to_ptr_version(std::uintptr_t versionedPtr, Ptr& outPtr, std::uint16_t& outVersion) {
	const std::uintptr_t value(versionedPtr);
	const std::uintptr_t ptrValue((value << 16) >> 16);
	const std::uintptr_t versionValue(value >> 48);
	outPtr = (Ptr)ptrValue;
	outVersion = (std::uint16_t)versionValue;
}

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

	static constexpr size_type Max_Node_Height = cpq_detail::log2ceil(ExpectedEntriesHint);

	/// <summary>
	/// concurrent_priority_queue node. Used to sequence items in the queue.
	/// </summary>
	class node
	{
	public:
		/// <summary>
		/// node constructor
		/// </summary>
		/// <param name="keyRef">Stores persistent reference to key_type</param>
		/// <param name="itemRef">Stores persistent reference to value_type</param>
		node(const key_type& keyRef, value_type& itemRef)
			: m_item(itemRef)
			, m_key(keyRef)
			, m_height(0)
			, m_next{nullptr}
		{}

	private:
		friend class concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>;

		// Using unsafe hint... what would be the hazards?

		// There is no way to guarantee that external inspectors (other threads) seeing ANY other value than we want 
		// in the 'hint' array.
		// If it is linked in to list at any time. Another thread may always stall while inspecting a node. 

		// What does it mean for the other thread? 
		// If this node is placed at the beginning 
		// Thread A inspects it, stalls.
		// Thread B pops it. 
		// Some time later, this node is reinserted.. At the BACK
		// What then? If thread A meant to insert after this node and had already compared against it's key?
		// Then thread A would load m_next and recheck key to make sure it is at least equal.. Else fail the insertion attempt.

		// One of the main issues, as I recall, is surrounding the front element. 
		// When that is dequeued you need TWO ops, first de-link it, then secure it from others linking after IT
		// In the concurrent_sorted_list I use helping to delink it, and then remove it from any other linking considerations. That's just the worst. Very chokey chokey concurrency.
		// Right, now I remember. Tag it's m_next to say 'this be mine'. And then have all interested parties delink it, while only the tagging party gets it...
		// Ugh it's so ugly.

		// During a pop we need to:
		// * cas m_next
		// * exchange the nextHint values that refer to next

		// During an insert we need to: 
		// * cas m_next of previous
		// * alter the m_nextHint of values with height <= inserted




		// Note:
		// A pop operation is inherently a DE-LINKING operation
		// while
		// An insert operation is a LINKING operation
		// 
		// This means both operations will contain multiple steps
		//... Blahrg.
		// Hmm. So, states? 
		// Delinked -> Linking -> Linked -> Delinking

	 	// This would mean at least 3 atomic ops to delink
		// and 4 atomic ops to link

		// So. Partial states. This would probably mean helping.... Yeah. 
		// For example:
		// head -> 1 -> 2 -> 3
		// Thread A attempts to remove 1. 
		// Thread B attempts insert 1 -> B -> 2...
		// Thread B must exchange the relevant references to 2 in 1
		// Thread A must remove references to 1 in head.

		// While the m_next of 1 may be flagged using a tag-bit
		// The m_nextHint may not be.
		// In the scenario that Thread B wants to insert 1 -> 2 -> B -> 3 and the inserted node refers to 1 as previous at some height,
		// this means there will be no synchronization in m_next...

		// To GUARANTEE that if we swap the m_nextHint in 1 -> 3 when it has not been partially or wholly removed, we need to re-check after we insert.

		// The original idea about having the hints is that we may do a somewhat bonked-out search in the upper levels, knowing that whatever happens, once we get
		// to m_next it is always correct. How can we guarantee this though? 
		// What would be the hazards?
		// In the last mentioned scenario, B could swap m_nextHint in 1 from 3 to B. Then, a partially stale node would refer to B. This will be fine, since that node will
		// be going out of scope (probably not. I missed some causality of course), and then B would be found from m_next anyway.

		// The true hazards would be if we had a stale reference to a node that was not participating in the structure. This could mean that we end up in a dead-end. 
		// What would provoke this? Since we are only allowing removals in the 1st spot, and node references are -> forward, it seems the only way to refer to a stale node
		// is by removing a node that comes after another. 
		// SO, inserting in the 1st spot? 
		// We would naturally know to insert 1st if current 1st > insert. In the event a node is inserted at 1st, it must attempt to link in at head. This linking will begin at... bottom? top? Bottom.
		// So, when the on-going removal of previous 1st progresses, it will fail CAS of m_next. Then we will have a partially removed node in 2nd place. 


		// Combination of hazard pointer, accessorsCount, node ref count, atomic m_next ... This ought be slow AF
		// Discreet find, try_insert(findResult), remove(findResult?) 

		// Massive synchronization traffic expected in the front of queue. Head will hold references to first node. Perhaps we can force this to only be bottom... 
		// 



	public:
		value_type& m_item;

		std::uint8_t m_height;
		const key_type m_key;

	private:
		// It would seem that the atomic property & memfence does not add THAT much overhead. Rather, almost all of the overhead is attributed to cache-traffic.
		std::atomic<std::uintptr_t> m_next[Max_Node_Height];

	};

	concurrent_priority_queue();

	/// <summary>
	/// Insert an item in to the queue
	/// </summary>
	/// <param name="item">Key value pair to insert</param>
	void insert(node* toInsert);

	/// <summary>
	/// Attempt to retrieve the first item
	/// </summary>
	/// <param name="out">The out value item</param>
	/// <returns>Indicates if the operation was successful</returns>
	bool try_pop(node*& out);
private:

	static constexpr std::uintptr_t Ptr_Mask = ~(std::uintptr_t(std::numeric_limits<std::uint16_t>::max()) << 48);
	static constexpr std::uintptr_t Version_Mask = ~Ptr_Mask;
	static constexpr std::uintptr_t Version_Step = Ptr_Mask + 1;

	node m_head;
	node m_end;

	comparator_type m_comparator;
};
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::concurrent_priority_queue()
	: m_head()
	, m_end()
	, m_comparator()
{
	for (node* item = std::begin(m_begin.m_nextHint); item != std::end(m_begin.m_nextHint); ++item)
		item = &m_end;

	m_begin.m_next.store((std::uintptr_t) & m_end, std::memory_order_relaxed);
}

template<class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint, class Allocator, class Comparator>
inline bool concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>::try_pop(node*& out)
{
	std::uintptr_t head(m_head.load(std::memory_order_acquire));

	while (head & Ptr_Mask) {
		node* const headPtr((node*)head & Ptr_Mask);
		const std::uint16_t headVersion(head & ~Ptr_Mask);
		const std::uint16_t nextHeadVersion((headVersion + Version_Step) & Version_Mask);

		const std::uintptr_t next(headPtr->m_next.load(std::memory_order_relaxed);
		const std::uintptr_t desired((next & Ptr_Mask) | nextHeadVersion);

		if (m_head.compare_exchange_weak(head, desired, std::memory_order_acquire, std::memory_order_relaxed)) {
			out = headPtr;
			return true;
		}
	};

	return false;
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