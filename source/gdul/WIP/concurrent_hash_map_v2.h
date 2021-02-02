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

#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/memory/thread_local_member.h>

#include <xhash>
#include <iterator>

#pragma warning(push)
// Anonymous struct
#pragma warning(disable:4201)

namespace gdul {

namespace chm_detail {
using size_type = std::size_t;

constexpr size_type Default_Capacity = 32;
constexpr std::uint8_t Growth_Multiple = 2;

constexpr size_type to_bucket_count(size_type desiredCapacity)
{
	return desiredCapacity * Growth_Multiple;
}

template <class Map>
struct iterator;
template <class Map>
struct const_iterator;

enum item_flag : std::uint8_t
{
	item_flag_null = 0,
	item_flag_empty = 0,
	item_flag_valid = 1 << 0,
	item_flag_deleted = 1 << 1,
	item_flag_disposed = 1 << 2,
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
	using iterator = chm_detail::iterator<concurrent_hash_map<Key, Value, Hash, Allocator>>;
	using const_iterator = chm_detail::const_iterator<concurrent_hash_map<Key, Value, Hash, Allocator>>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

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
	/// Iterator to first item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	iterator begin() { return iterator(this, scan_for_item(nullptr, 1).kv); }

	/// <summary>
	/// Iterator to first item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	const_iterator begin() const { return const_iterator(this, scan_for_item(nullptr, 1).kv); }

	/// <summary>
	/// Iterator to last item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	reverse_iterator rbegin() { return reverse_iterator(iterator(this, scan_for_item(nullptr, -1).kv)); }

	/// <summary>
	/// Iterator to last item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	const_reverse_iterator rbegin() const { return reverse_iterator(const_iterator(this, scan_for_item(nullptr, -1).kv)); }

	/// <summary>
	/// Iterator to end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	iterator end() { return iterator(this, nullptr); }

	/// <summary>
	/// Iterator to end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	const_iterator end() const { return const_iterator(this, nullptr); }

	/// <summary>
	/// Iterator to reverse end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	reverse_iterator rend() { return reverse_iterator(end()); }

	/// <summary>
	/// Iterator to reverse end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	const reverse_iterator rend() const { return reverse_iterator(end()); }

	/// <summary>
	/// Attempt deletion of item
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>True if item was present, and deleted</returns>
	bool unsafe_erase(Key k);

private:
	union packed_item_ptr
	{
		chm_detail::item_flag state;
		item_type* item;
	};

	using hash_array = shared_ptr<std::atomic<std::size_t>[]>;
	using item_array = shared_ptr<std::atomic<item_type*>[]>;
	struct bucket_pack
	{
		hash_array hashes;
		item_array items;
	};

	struct t_container
	{
		// For ownership
		shared_ptr<bucket_pack> buckets;

		std::atomic<std::size_t>* hashes;
		std::atomic<item_type*>* items;

		size_type bucketCount;
	};

	friend struct iterator;
	friend struct const_iterator;

	template <class In>
	std::pair<iterator, bool> insert_internal(In&& in);

	bool refresh_tl(t_container& tContainer) const;

	void grow_buckets(t_container& tContainer);

	static shared_ptr<bucket_pack> allocate_bucket_pack(size_type size, allocator_type alloc);

	std::pair<iterator, bool> try_insert_in_bucket_array(bucket_pack& buckets, item_type* item, std::size_t hash);

	static size_type find_index(const item_array& items, size_type itemCount, std::size_t hash);

	typename packed_item_ptr scan_for_item(const item_type* from, int direction) const;

	static void help_dispose_buckets(t_container& tContainer);
	static void dispose_bucket(std::atomic<packed_item_ptr>& item);

	concurrent_guard_pool<item_type, allocator_type> m_pool;

	atomic_shared_ptr<bucket_pack> m_items;

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
	: m_pool((std::uint32_t)chm_detail::to_bucket_count(align_value_pow2(capacity)), (std::uint32_t)chm_detail::to_bucket_count(align_value_pow2(capacity)) / 1, alloc)
	, m_items(gdul::allocate_shared<bucket_type[]>(chm_detail::to_bucket_count(align_value_pow2(capacity)), alloc))
	, t_items(alloc, t_container{ m_items.load(), chm_detail::to_bucket_count(align_value_pow2(capacity)) })
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
	const std::size_t hash(hash_type()(k));

	t_container& tl(t_items);

	do {
		bucket_array& buckets(tContainer.buckets);

		const size_type index(find_index(tl.items, tl.bucketCount, hash));

		if (index != tContainer.bucketCount) {

			std::atomic_thread_fence(std::memory_order_acquire);

			bucket_items_type bucket(buckets[index].my_val());

			if (bucket.packedPtr.state & chm_detail::item_flag_valid) {
				bucket.packedPtr.state = chm_detail::item_flag_null;

				return iterator(bucket.packedPtr.kv);
			}
		}

	} while (refresh_tl(tContainer));

	return end();
}
template<class Key, class Value, class Hash, class Allocator>
inline const typename concurrent_hash_map<Key, Value, Hash, Allocator>::const_iterator concurrent_hash_map<Key, Value, Hash, Allocator>::find(Key k) const
{
	const std::size_t hash(hash_type()(k));
	const t_container& tContainer(t_items);

	do {
		const bucket_array& tBuckets(tContainer.buckets);
		const size_type index(find_index(tBuckets, hash));

		if (index != tContainer.bucketCount) {

			std::atomic_thread_fence(std::memory_order_acquire);

			bucket_items_type bucket(tBuckets[index].my_val());

			if (bucket.packedPtr.state & chm_detail::item_flag_valid) {
				bucket.packedPtr.state = chm_detail::item_flag_null;

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
	t_container& tl(t_items);

	refresh_tl(tl);

	bucket_array& buckets(tl.buckets);

	const size_type index(find_index(tl, hash_type()(k)));

	if (index == tl.bucketCount) {
		return false;
	}

	bucket_items_type& bucket(buckets[index].my_val());

	if (!(bucket.packedPtr.state & chm_detail::item_flag_valid)) {
		return false;
	}

	bucket.packedPtr.stateValue &= ~chm_detail::item_flag_valid;
	bucket.packedPtr.stateValue |= chm_detail::item_flag_deleted;

	std::atomic_thread_fence(std::memory_order_release);

	typename bucket_items_type::packed_ptr ptr(bucket.packedPtr);
	ptr.state = chm_detail::item_flag_null;

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
		bucket_pack& buckets(*tContainer.buckets.get());

		std::pair<iterator, bool> result(try_insert_in_bucket_array(buckets, item, hash));

		if (result.first != end()) {

			if (result.second) {
				if (!((m_size++ * chm_detail::Growth_Multiple) < tContainer.bucketCount)) {
					grow_buckets(tContainer);
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

		grow_buckets(tContainer);
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type concurrent_hash_map<Key, Value, Hash, Allocator>::find_index(const concurrent_hash_map<Key, Value, Hash, Allocator>::hash_array& items, size_type itemCount, typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type hash)
{
	// Scan for hash... 
	assert(false);

	for (size_type probe(0); probe < itemCount; ++probe) {
		const size_type index((hash + (probe * probe)) % itemCount);

		const packed_item_ptr item(items[index].load(std::memory_order_acquire));

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
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::packed_item_ptr concurrent_hash_map<Key, Value, Hash, Allocator>::scan_for_item(const item_type* from, int direction) const
{
	t_container& tl(t_items);

	refresh_tl(tl);

	size_type index(from ? find_index(tl.items, tl.bucketCount, hash_type()(from->first)) : (tl.bucketCount - 1 + direction) % tl.bucketCount);

	packed_item_ptr result;
	result.item = nullptr;


	for (index = index + direction; index < tl.bucketCount; index += direction) {
		packed_item_ptr at(tl.items[index].load(std::memory_order_acquire));
		if (at.state & chm_detail::item_flag_valid) {
			at.state = 0;
			result = at;
			break;
		}
	}

	return result;
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
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::grow_buckets(typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer)
{
	help_dispose_buckets(tContainer);

	shared_ptr<bucket_pack> currentPack(m_arrays.load(std::memory_order_acquire));

	if (tContainer.bucketCount < currentPack->items.item_count()) {
		tContainer.items = currentPack->items.get();
		tContainer.hashes = currentPack->hashes.get();
		tContainer.buckets = std::move(currentPack);
		tContainer.bucketCount = tContainer.buckets.item_count();
		return;
	}

	shared_ptr<bucket_pack> newArrays(allocate_bucket_pack(currentArray.item_count() * 2, m_allocator));

	std::atomic_thread_fence(std::memory_order_acquire);

	for (size_type i = 0; i < currentArray.item_count(); ++i) {
		packed_item_ptr item(currentArray[i].my_val());
		item.state = chm_detail::item_flag_null;

		try_insert_in_bucket_array(newArray, item.kv, items.hash);
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
inline shared_ptr<typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_pack> concurrent_hash_map<Key, Value, Hash, Allocator>::allocate_bucket_pack(size_type size, typename concurrent_hash_map<Key, Value, Hash, Allocator>::allocator_type alloc)
{
	shared_ptr<bucket_pack> pack(gdul::allocate_shared<bucket_pack>(alloc));
	pack->items = gdul::allocate_shared<std::atomic<packed_item_ptr>[]>(size, alloc));
	pack->hashes = gdul::allocate_shared<std::atomic<std::size_t>>(size, alloc));
}

template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::try_insert_in_bucket_array(typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_pack& buckets, typename concurrent_hash_map<Key, Value, Hash, Allocator>::item_type* item, std::size_t hash)
{
	for (std::size_t retry = 0; retry < 10; ++retry) {

		// No good  here, inconsistent buckets vs t_items used.. 
		assert(false);

		const size_type index(find_index(t_items, hash));

		if (index == buckets->items.item_count()) {
			return std::make_pair(end(), false);
		}

		packed_item_ptr existingItem(buckets->items[index].load(std::memory_order_acquire));

		if (existingItem.state & chm_detail::item_flag_disposed) {
			return std::make_pair(end(), false);
		}

		// Sequencing hash load after item load? .. 
		std::size_t existingHash(buckets->hashes[index].load(std::memory_order_acquire));

		if (existingHash == hash) {
			existingItem.state = chm_detail::item_flag_null;
			return std::make_pair(iterator(this, existingItem.item), false);
		}

		// if state is null, try to put our item here
		if (existingItem.state == chm_detail::item_flag_null) {
			packed_item_ptr desired;
			desired.item = item;
			desired.state = chm_detail::item_flag_valid;

			if (buckets[index].compare_exchange_strong(existingItem, desired)) {
				existingItem = desired;
			}
		}

		existingItem.state = chm_detail::item_flag_null;

		const std::size_t desiredHash(hash_type()(existingItem.item->first));

		// Try to insert whatever hash matches the existing item
		buckets->hashes[index].compare_exchange_strong(existingHash, desiredHash, std::memory_order_release, std::memory_order_relaxed);

		// If the item was ours, success!
		if (existingItem.item == item) {
			return std::make_pair(iterator(this, item), true);
		}
	}
	return std::make_pair(end(), false);
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::help_dispose_buckets(typename concurrent_hash_map<Key, Value, Hash, Allocator>::t_container& tContainer)
{
	for (size_type i = 0; i < tContainer.bucketCount; ++i) {
		dispose_bucket(tContainer.items[i]);
	}
}
template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_hash_map<Key, Value, Hash, Allocator>::dispose_bucket(std::atomic<typename concurrent_hash_map<Key, Value, Hash, Allocator>::packed_item_ptr>& item)
{
	packed_item_ptr expected(item.load(std::memory_order_relaxed));

	while (!(expected.state & chm_detail::item_flag_disposed)) {
		packed_item_ptr desired(expected);
		desired.state |= chm_detail::item_flag_disposed;

		if (item.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_acquire)) {
			break;
		}
	}
}
namespace chm_detail {
template <class Map>
struct iterator_base
{
	using iterator_category = std::bidirectional_iterator_tag;
	using value_type = typename Map::item_type;
	using difference_type = std::ptrdiff_t;
	using pointer = value_type*;
	using reference = value_type&;

};

template <class Map>
struct iterator : public iterator_base<Map>
{
	using iterator_base<Map>::iterator_base;

	iterator()
		: m_at(nullptr)
	{
	}

	iterator(const iterator<Map>& itr)
		: iterator(itr.m_parent, itr.m_at)
	{
	}

	iterator(Map* parent, typename iterator_base<Map>::value_type* at)
		: m_at(at)
		, m_parent(parent)
	{
	}

	const typename iterator_base<Map>::value_type& operator*() const
	{
		return m_at;
	}

	const typename iterator_base<Map>::value_type* operator->() const
	{
		return &m_at;
	}

	bool operator==(const iterator<Map>& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const iterator<Map>& other) const
	{
		return !operator==(other);
	}

	bool operator==(const const_iterator<Map>& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const const_iterator<Map>& other) const
	{
		return !operator==(other);
	}

	iterator operator++()
	{
		m_at = m_parent->scan_for_item(m_at, 1).kv;
		return *this;
	}
	iterator operator--()
	{
		m_at = m_parent->scan_for_item(m_at, -1).kv;
		return *this;
	}

	typename iterator_base<Map>::value_type& operator*()
	{
		return *m_at;
	}

	typename iterator_base<Map>::value_type* operator->()
	{
		return m_at;
	}

	typename iterator_base<Map>::value_type* m_at;

private:
	friend struct const_iterator<Map>;

	Map* m_parent;
};
template <class Map>
struct const_iterator : public iterator_base<Map>
{
	using iterator_base<Map>::iterator_base;

	const_iterator()
		: m_at(nullptr)
	{
	}

	const_iterator(const const_iterator<Map>& itr)
		: m_at(itr.m_at)
		, m_parent(itr.m_parent)
	{
	}

	const_iterator(const Map* parent, const typename iterator_base<Map>::value_type* at)
		: m_at(at)
		, m_parent(parent)
	{
	}

	const typename iterator_base<Map>::value_type& operator*() const
	{
		return m_at;
	}

	const typename iterator_base<Map>::value_type* operator->() const
	{
		return &m_at;
	}

	bool operator==(const iterator<Map>& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const iterator<Map>& other) const
	{
		return !operator==(other);
	}

	bool operator==(const const_iterator<Map>& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const const_iterator<Map>& other) const
	{
		return !operator==(other);
	}

	const_iterator(iterator<Map> it)
		: m_at(it.m_at)
		, m_parent(it.m_parent)
	{
	}

	const_iterator operator++() const
	{
		m_at = m_parent->scan_for_item(m_at, 1).kv;
		return *this;
	}
	const_iterator operator--() const
	{
		m_at = m_parent->scan_for_item(m_at, -1).kv;
		return *this;
	}

	mutable const typename iterator_base<Map>::value_type* m_at;

private:
	friend struct iterator<Map>;

	mutable const Map* m_parent;
};
}
}
#pragma warning(pop)
