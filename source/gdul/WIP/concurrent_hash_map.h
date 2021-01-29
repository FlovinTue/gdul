//Copyright(c) 2020 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once


#include <xhash>
#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/memory/thread_local_member.h>
#include <gdul/memory/atomic_128.h>

#pragma warning(push)
// Anonymous struct
#pragma warning(disable:4201)

namespace gdul {

namespace chm_detail {
using size_type = std::size_t;

constexpr size_type Default_Capacity = 32;
constexpr std::uint8_t Growth_Multiple = 2;

template <class Key, class Value>
struct bucket;

constexpr size_type to_bucket_count(size_type desiredCapacity)
{
	return desiredCapacity * Growth_Multiple;
}

template <class Dummy = void>
inline size_type pow2_align(std::size_t from, std::size_t clamp = std::numeric_limits<size_type>::max())
{
	const std::size_t from_(from < 2 ? 2 : from);

	const float flog2(std::log2f(static_cast<float>(from_)));
	const float nextLog2(std::ceil(flog2));
	const float fNextVal(powf(2.f, nextLog2));

	const std::size_t nextVal(static_cast<size_t>(fNextVal));
	const std::size_t clampedNextVal((clamp < nextVal) ? clamp : nextVal);

	return clampedNextVal;
}

template <class Item>
struct iterator;
template <class Item>
struct const_iterator;

enum bucket_flag : std::uint8_t
{
	bucket_flag_null = 0,
	bucket_flag_empty = 0,
	bucket_flag_valid = 1 << 0,
	bucket_flag_deleted = 1 << 1,
	bucket_flag_disposed = 1 << 2,
};
}

template <class Key, class Value, class Hash = std::hash<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_hash_map
{
public:
	using size_type = typename chm_detail::size_type;
	using key_type = Key;
	using value_type = Value;
	using item_type = std::pair<key_type, value_type>;
	using hash_type = Hash;
	using allocator_type = Allocator;
	using iterator = chm_detail::iterator<item_type>;
	using const_iterator = chm_detail::const_iterator<const item_type>;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_hash_map();

	/// <summary>
	/// Constructor with allocator
	/// </summary>
	/// <param name="alloc">Allocator for array and items</param>
	concurrent_hash_map(Allocator alloc);

	/// <summary>
	/// Constructor with capacity
	/// </summary>
	/// <param name="capacity">Initial capacity of map</param>
	concurrent_hash_map(size_type capacity);

	/// <summary>
	/// Constructor with capacity and allocator
	/// </summary>
	/// <param name="capacity">Initial capacity of map</param>
	/// <param name="alloc">Allocator for array and items</param>
	concurrent_hash_map(size_type capacity, Allocator alloc);

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted</param>
	std::pair<iterator, bool> insert(const item_type& in);

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted using move semantics</param>
	std::pair<iterator, bool> insert(item_type&& in);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>Iterator to item on success. end on failure</returns>
	iterator find(Key k);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>Iterator to item on success. end on failure</returns>
	const const_iterator find(Key k) const;

	/// <summary>
	/// Search for key, and if needed inserts uninitialized item
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>Iterator to item</returns>
	iterator operator[](size_type k);

	/// <summary>
	/// Query for number of items in map
	/// </summary>
	/// <returns>Item count</returns>
	size_type size() const;

	/// <summary>
	/// Query for empty map
	/// </summary>
	/// <returns>True if no items are present</returns>
	bool empty() const;

	/// <summary>
	/// Query for map capacity
	/// </summary>
	/// <returns>Item capacity</returns>
	size_type capacity() const;

	/// <summary>
	/// Iterator to end
	/// </summary>
	/// <returns>Iterator</returns>
	iterator end() { return iterator(nullptr); }

	/// <summary>
	/// Iterator to end
	/// </summary>
	/// <returns>Iterator</returns>
	const_iterator end() const { return const_iterator(nullptr); }

	/// <summary>
	/// Attempt deletion of item
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>True if item was present, and deleted</returns>
	bool unsafe_erase(Key k);

private:
	using bucket_items_type = chm_detail::bucket<key_type, value_type>;
	using bucket_type = atomic_128<bucket_items_type>;
	using bucket_array = shared_ptr<bucket_type[]>;

	struct t_container
	{
		bucket_array buckets;
		// Keep this here as a small perf opt
		size_type bucketCount;
	};

	template <class In>
	std::pair<iterator, bool> insert_internal(In&& in);

	bool refresh_tl(t_container& tContainer) const;

	void grow_bucket_array(t_container& tContainer);

	std::pair<iterator, bool> try_insert_in_bucket_array(bucket_array& buckets, item_type* item, size_type hash);

	static size_type find_index(const t_container& tContainer, size_type hash);
	static void help_dispose_bucket_array(t_container& tContainer);
	static void dispose_bucket(bucket_type& bucket);

	concurrent_guard_pool<item_type, allocator_type> m_pool;

	atomic_shared_ptr<bucket_type[]> m_items;

	mutable tlm<t_container, allocator_type> t_items;

	std::atomic<size_type> m_size;

	allocator_type m_allocator;
};
template<class Key, class Value, class Hash, class Allocator>
inline concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map()
	: concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(chm_detail::Default_Capacity, Allocator())
{
}
template<class Key, class Value, class Hash, class Allocator>
inline concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(Allocator alloc)
	: concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(chm_detail::Default_Capacity, alloc)
{
}
template<class Key, class Value, class Hash, class Allocator>
inline concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(size_type capacity)
	: concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(capacity, Allocator())
{
}
template<class Key, class Value, class Hash, class Allocator>
inline concurrent_hash_map<Key, Value, Hash, Allocator>::concurrent_hash_map(size_type capacity, Allocator alloc)
	: m_pool((std::uint32_t)chm_detail::to_bucket_count(chm_detail::pow2_align<>(capacity)), (std::uint32_t)chm_detail::to_bucket_count(chm_detail::pow2_align<>(capacity)) / 1, alloc)
	, m_items(gdul::allocate_shared<bucket_type[]>(chm_detail::to_bucket_count(chm_detail::pow2_align<>(capacity)), alloc))
	, t_items(alloc, t_container{ m_items.load(), chm_detail::to_bucket_count(chm_detail::pow2_align<>(capacity)) })
	, m_size(0)
	, m_allocator(alloc)
{
}
template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::insert(const item_type& in)
{
	return insert_internal(std::move(in));
}
template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::insert(item_type&& in)
{
	return insert_internal(std::move(in));
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator concurrent_hash_map<Key, Value, Hash, Allocator>::find(Key k)
{
	const std::uint64_t hash(hash_type()(k));

	t_container& tContainer(t_items);

	do {
		bucket_array& buckets(tContainer.buckets);

		const size_type index(find_index(tContainer, hash));

		if (index != tContainer.bucketCount) {

			std::atomic_thread_fence(std::memory_order_acquire);

			bucket_items_type bucket(buckets[index].my_val());

			if (bucket.packedPtr.state & chm_detail::bucket_flag_valid) {
				bucket.packedPtr.state = chm_detail::bucket_flag_null;

				return iterator(bucket.packedPtr.kv);
			}
		}

	} while (refresh_tl(tContainer));

	return end();
}
template<class Key, class Value, class Hash, class Allocator>
inline const typename concurrent_hash_map<Key, Value, Hash, Allocator>::const_iterator concurrent_hash_map<Key, Value, Hash, Allocator>::find(Key k) const
{
	const std::uint64_t hash(hash_type()(k));
	const t_container& tContainer(t_items);

	do {
		const bucket_array& tBuckets(tContainer.buckets);
		const size_type index(find_index(tBuckets, hash));

		if (index != tContainer.bucketCount) {

			std::atomic_thread_fence(std::memory_order_acquire);

			bucket_items_type bucket(tBuckets[index].my_val());

			if (bucket.packedPtr.state & chm_detail::bucket_flag_valid) {
				bucket.packedPtr.state = chm_detail::bucket_flag_null;

				return const_iterator(bucket.packedPtr.kv);
			}
		}

	} while (refresh_tl(tContainer));

	return end();
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator concurrent_hash_map<Key, Value, Hash, Allocator>::operator[](size_type k)
{
	iterator item(find(k));

	if (item != end()) {
		return item;
	}

	return insert(std::make_pair(Key(), Value())).first;
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type concurrent_hash_map<Key, Value, Hash, Allocator>::size() const
{
	return m_size.load(std::memory_order_relaxed);
}
template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_hash_map<Key, Value, Hash, Allocator>::empty() const
{
	return !size();
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type concurrent_hash_map<Key, Value, Hash, Allocator>::capacity() const
{
	const t_container& tContainer(t_items);

	refresh_tl(tContainer);

	return tContainer.bucketCount / chm_detail::Growth_Multiple;
}
template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_hash_map<Key, Value, Hash, Allocator>::unsafe_erase(Key k)
{
	t_container& tContainer(t_items);

	refresh_tl(tContainer);

	bucket_array& buckets(tContainer.buckets);

	const size_type index(find_index(tContainer, hash_type()(k)));

	if (index == tContainer.bucketCount) {
		return false;
	}

	bucket_items_type& bucket(buckets[index].my_val());

	if (!(bucket.packedPtr.state & chm_detail::bucket_flag_valid)) {
		return false;
	}

	bucket.packedPtr.stateValue &= ~chm_detail::bucket_flag_valid;
	bucket.packedPtr.stateValue |= chm_detail::bucket_flag_deleted;

	std::atomic_thread_fence(std::memory_order_release);

	typename bucket_items_type::packed_ptr ptr(bucket.packedPtr);
	ptr.state = chm_detail::bucket_flag_null;

	m_pool.recycle(ptr.kv);

	return true;
}
template<class Key, class Value, class Hash, class Allocator>
template<class In>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::insert_internal(In&& in)
{
	item_type* const item(m_pool.get());
	*item = std::forward<In>(in);

	const size_type hash(hash_type()(in.first));

	for (;;) {
		t_container& tContainer(t_items);
		bucket_array& buckets(tContainer.buckets);

		std::pair<iterator, bool> result(try_insert_in_bucket_array(buckets, item, hash));

		if (result.first != end()) {

			if (result.second) {
				if (!((m_size++ * chm_detail::Growth_Multiple) < tContainer.bucketCount)) {
					grow_bucket_array(tContainer);
				}
			}
			else {
				m_pool.recycle(item);
			}

			return result;
		}

		if (refresh_tl(tContainer)) {
			continue;
		}

		grow_bucket_array(tContainer);
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type concurrent_hash_map<Key, Value, Hash, Allocator>::find_index(const typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer, typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type hash)
{
	const size_type bucketCount(tContainer.bucketCount);

	for (size_type probe(0); probe < bucketCount; ++probe) {
		const size_type index((hash + (probe * probe)) % bucketCount);

		const bucket_type& bucket(tContainer.buckets[index]);
		const bucket_items_type& bucketItems(bucket.my_val());
		const size_type bucketHash(bucketItems.hash);

		if (bucketHash == hash) {
			return index;
		}

		if (!bucketHash) {
			return index;
		}
	}

	return bucketCount;
}
template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_hash_map<Key, Value, Hash, Allocator>::refresh_tl(typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer) const
{
	if (tContainer.buckets == m_items) {
		return false;
	}

	tContainer.buckets = m_items.load(std::memory_order_acquire);
	tContainer.bucketCount = tContainer.buckets.item_count();

	return true;
}
template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::grow_bucket_array(typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer)
{
	help_dispose_bucket_array(tContainer);

	bucket_array currentArray(m_items.load());

	if (tContainer.bucketCount < currentArray.item_count()) {
		tContainer.buckets = std::move(currentArray);
		tContainer.bucketCount = tContainer.buckets.item_count();
		return;
	}

	assert(currentArray.item_count() < std::numeric_limits<typename bucket_array::size_type>::max() && "Bucket array overflow");

	bucket_array newArray(gdul::allocate_shared<bucket_type[]>(currentArray.item_count() * 2, m_allocator));

	std::atomic_thread_fence(std::memory_order_acquire);

	for (size_type i = 0; i < currentArray.item_count(); ++i) {
		bucket_items_type items(currentArray[i].my_val());
		items.packedPtr.state = chm_detail::bucket_flag_null;

		try_insert_in_bucket_array(newArray, items.packedPtr.kv, items.hash);
	}

	if (m_items.compare_exchange_strong(currentArray, newArray)) {
		tContainer.buckets = std::move(newArray);
		tContainer.bucketCount = tContainer.buckets.item_count();
	}
	else {
		tContainer.buckets = std::move(currentArray);
		tContainer.bucketCount = tContainer.buckets.item_count();
	}
}
template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::try_insert_in_bucket_array(typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_array& buckets, typename concurrent_hash_map<Key, Value, Hash, Allocator>::item_type* item, typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type hash)
{
	const size_type index(find_index(t_items, hash));

	if (index == buckets.item_count()) {
		return std::make_pair(end(), false);
	}

	std::atomic_thread_fence(std::memory_order_acquire);

	bucket_items_type items(buckets[index].my_val());

	if (items.packedPtr.state & chm_detail::bucket_flag_disposed) {
		return std::make_pair(end(), false);
	}

	if (items.packedPtr.state & chm_detail::bucket_flag_valid) {
		items.packedPtr.state = chm_detail::bucket_flag_null;
		return std::make_pair(items.packedPtr.kv, false);
	}

	bucket_items_type desired;
	desired.hash = hash;
	desired.packedPtr.kv = item;
	desired.packedPtr.state = chm_detail::bucket_flag_valid;

	if (buckets[index].compare_exchange_strong(items, desired)) {
		return std::make_pair(iterator(item), true);
	}

	return std::make_pair(end(), false);
}
template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::help_dispose_bucket_array(typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer)
{
	for (size_type i = 0; i < tContainer.bucketCount; ++i) {
		dispose_bucket(tContainer.buckets[i]);
	}
}
template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::dispose_bucket(typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_type& bucket)
{
	bucket_items_type items(bucket.my_val());

	while (!(items.packedPtr.state & chm_detail::bucket_flag_disposed)) {
		bucket_items_type disposedItems(items);
		disposedItems.packedPtr.stateValue |= chm_detail::bucket_flag_disposed;

		if (bucket.compare_exchange_strong(items, disposedItems)) {
			break;
		}
	}
}
namespace chm_detail {
template <class Key, class Value>
struct alignas(16) bucket
{
	std::uint64_t hash = 0;

	union packed_ptr
	{
		std::pair<Key, Value>* kv = nullptr;

		struct
		{
			std::uint8_t padding[6];

			union
			{
				bucket_flag state;
				std::uint8_t stateValue;
			};
		};

	}packedPtr;
};
template <class Item>
struct iterator_base
{
	using iterator_tag = std::bidirectional_iterator_tag;
	using key_type = typename Item::first_type;
	using value_type = typename Item::second_type;

	iterator_base()
		: m_at(nullptr)
	{
	}

	iterator_base(const iterator_base<Item>& itr)
		: iterator_base(itr.m_at)
	{
	}

	iterator_base(Item* b)
		: m_at(b)
	{
	}

	const std::pair<key_type, value_type>& operator*() const
	{
		return m_at;
	}

	const std::pair<key_type, value_type>* operator->() const
	{
		return &m_at;
	}

	bool operator==(const iterator_base& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const iterator_base& other) const
	{
		return !operator==(other);
	}

	Item* m_at;
};

template <class Item>
struct iterator : public iterator_base<Item>
{
	using iterator_base<Item>::iterator_base;

	//iterator operator++()
	//{
	//	return { this->m_at->m_linkViews[0].load(std::memory_order_relaxed).m_Bucket };
	//}

	std::pair<iterator_base<Item>::key_type, iterator_base<Item>::value_type>& operator*()
	{
		return *this->m_at;
	}

	std::pair<iterator_base<Item>::key_type, iterator_base<Item>::value_type>* operator->()
	{
		return this->m_at;
	}
};
template <class Item>
struct const_iterator : public iterator_base<Item>
{
	using iterator_base<Item>::iterator_base;

	const_iterator(iterator<std::remove_const_t<Item>> fi)
		: iterator_base<Item>(fi.m_at)
	{
	}

	//const_iterator operator++() const
	//{
	//	return { this->m_at->m_linkViews[0].load(std::memory_order_relaxed).m_Bucket };
	//}
};
}
}
#pragma warning(pop)
