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

template <class PtrType>
class packed_ptr;

template <class KeyType, class ValueType, size_type Height>
class node_impl;

static constexpr std::uintptr_t Ptr_Mask = (std::uintptr_t(1) << 45) - std::uintptr_t(1);
static constexpr std::uintptr_t Tag_Mask = (std::uintptr_t(1) << 45);
static constexpr std::uintptr_t Version_Mask = ~(Ptr_Mask | Tag_Mask);
static constexpr std::uintptr_t Version_Step = Tag_Mask << 1;

}
/// <summary>
/// ExpectedEntriesHint should be somewhere between the average and the maximum number of entries
/// contained within the queue at any given time. It is used to calculate the maximum node height 
/// in the skip-list.
/// </summary>
template <class KeyType, class ValueType, cpq_detail::size_type ExpectedEntriesHint = 64, class Allocator = std::allocator<std::uint8_t>, class Comparator = std::greater_equal<KeyType>>
class concurrent_priority_queue
{
	class node : public cpq_detail::node_impl<KeyType, ValueType, Max_Node_Height>
	{
	public:
		using m_item;
		using m_key;

	private:
		friend class concurrent_priority_queue<KeyType, ValueType, ExpectedEntriesHint, Allocator, Comparator>;

		using m_next;
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

	/// <summary>
	/// Insert an item in to the queue
	/// </summary>
	/// <param name="item">Key value pair to insert</param>
	void insert(node_type* toInsert);

	/// <summary>
	/// Attempt to retrieve the first item
	/// </summary>
	/// <param name="out">The out value item</param>
	/// <returns>Indicates if the operation was successful</returns>
	bool try_pop(node_type*& out);
private:

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
template <class PtrType>
class packed_ptr
{
public:
	packed_ptr();
	packed_ptr(std::uintptr_t value);

	PtrType* get_ptr();
	const PtrType* get_ptr() const;

	void set_ptr(PtrType* ptr);

	std::uint32_t get_version() const;
	void inc_version();

	bool get_tag() const;
	void set_tag(bool val) const;


private:
	std::uintptr_t to_packed_ptr(void* ptr, std::uint16_t version) {
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

	std::uintptr_t m_ptr;
};
template<class PtrType>
inline packed_ptr<PtrType>::packed_ptr()
	: m_ptr(0)
{}
template<class PtrType>
inline packed_ptr<PtrType>::packed_ptr(std::uintptr_t value)
	: m_ptr(value)
{}
template<class PtrType>
inline PtrType* packed_ptr<PtrType>::get_ptr()
{
	return (PtrType*)(m_ptr & Ptr_Mask) << 3;
}
template<class PtrType>
inline const PtrType* packed_ptr<PtrType>::get_ptr() const
{
	return (PtrType*)(m_ptr & Ptr_Mask) << 3;
}

template<class PtrType>
inline void packed_ptr<PtrType>::set_ptr(PtrType* ptr)
{
	const std::uintptr_t value((std::uintptr_t)ptr);
	const std::uintptr_t shifted(value >> 3);
	m_ptr &= ~(Ptr_Mask);
	m_ptr |= shifted;
}

template<class PtrType>
inline std::uint32_t packed_ptr<PtrType>::get_version() const
{
	return (std::uint32_t)(m_ptr >> 46);
}

template<class PtrType>
inline void packed_ptr<PtrType>::inc_version()
{
	m_ptr += Version_Step;
}

template<class PtrType>
inline bool packed_ptr<PtrType>::get_tag() const
{
	return m_ptr & Tag_Mask;
}

template<class PtrType>
inline void packed_ptr<PtrType>::set_tag(bool val) const
{
	m_ptr &= ~Tag_Mask;
	m_ptr |= (Tag_Mask * val);
}

/// <summary>
/// concurrent_priority_queue node. Used to sequence items in the queue.
/// </summary>
template <class KeyType, class ValueType, size_type Height>
class node_impl
{
public:
	using key_type = KeyType;
	using value_type = ValueType;

	/// <summary>
	/// node constructor
	/// </summary>
	node_impl()
		: m_item()
		, m_key()
		, m_height(0)
		, m_next{ nullptr }
	{}

	value_type m_item;
	const key_type m_key;

	std::atomic<std::uintptr_t> m_next[Max_Node_Height];
};

std::random_device random::s_rd;
thread_local std::mt19937 random::t_rng(random::s_rd());
}
}