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
#include <gdul/memory/pool_allocator.h>
#include <gdul/memory/thread_local_member.h>
#include <gdul/memory/atomic_128.h>
#include <gdul/utility/packed_ptr.h>
#include <gdul/math/math.h>

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
	item_flag_valid = 1 << 0,
	item_flag_deleted = 1 << 1,
	item_flag_disposed = 1 << 2,
};
}

/// <summary>
/// Concurrency safe lock-free hash map. Uses open addressing with quadratic probing
/// </summary>
/// <typeparam name="Key">Key type</typeparam>
/// <typeparam name="Value">Value type</typeparam>
/// <typeparam name="Hash">Hashing type</typeparam>
/// <typeparam name="Allocator">Allocator type</typeparam>
template <class Key, class Value, class Hash = std::hash<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_unordered_map
{
public:
	using size_type = typename chm_detail::size_type;
	using key_type = Key;
	using value_type = Value;
	using item_type = std::pair<key_type, value_type>;
	using hash_type = Hash;
	using allocator_type = Allocator;
	using iterator = chm_detail::iterator<concurrent_unordered_map<Key, Value, Hash, Allocator>>;
	using const_iterator = chm_detail::const_iterator<concurrent_unordered_map<Key, Value, Hash, Allocator>>;
	using reverse_iterator = std::reverse_iterator<iterator>;
	using const_reverse_iterator = std::reverse_iterator<const_iterator>;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_unordered_map();

	/// <summary>
	/// Constructor with allocator
	/// </summary>
	/// <param name="alloc">Allocator for array and items</param>
	concurrent_unordered_map(Allocator alloc);

	/// <summary>
	/// Constructor with capacity
	/// </summary>
	/// <param name="capacity">Initial capacity</param>
	concurrent_unordered_map(size_type capacity);

	/// <summary>
	/// Constructor with capacity and allocator
	/// </summary>
	/// <param name="capacity">Initial capacity</param>
	/// <param name="alloc">Allocator for array and items</param>
	concurrent_unordered_map(size_type capacity, Allocator alloc);

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted</param>
	/// <returns>Iterator to item and insertion result</returns>
	std::pair<iterator, bool> insert(const item_type& in);

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted using move semantics</param>
	/// <returns>Iterator to item and insertion result</returns>
	std::pair<iterator, bool> insert(item_type&& in);

	/// <summary>
	/// Insert an item using in place construction
	/// </summary>
	/// <typeparam name="...Args">Constructor argument types</typeparam>
	/// <param name="...args">Constructor arguments</param>
	/// <returns>Iterator to item and insertion result</returns>
	template <class ...Args>
	std::pair<iterator, bool> emplace(Args&&... args);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>Iterator to item on success. end on failure</returns>
	iterator find(Key key);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>Iterator to item on success. end on failure</returns>
	const const_iterator find(Key key) const;

	/// <summary>
	/// Search for key, and if needed inserts uninitialized item
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>Item value</returns>
	value_type& operator[](Key key);

	/// <summary>
	/// Access item at key, and if needed inserts uninitialized item
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>Item value</returns>
	value_type& at(Key key);

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
	/// The number of internal buckets
	/// </summary>
	/// <returns>Bucket count</returns>
	size_type bucket_count() const noexcept;

	/// <summary>
	/// Iterator to first item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	iterator begin();

	/// <summary>
	/// Iterator to first item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	const_iterator begin() const;

	/// <summary>
	/// Iterator to last item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	reverse_iterator rbegin();

	/// <summary>
	/// Iterator to last item. Concurrency safe, but item order may change if bucket array grows
	/// </summary>
	/// <returns>Iterator</returns>
	const_reverse_iterator rbegin() const;

	/// <summary>
	/// Iterator to end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	iterator end();

	/// <summary>
	/// Iterator to end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	const_iterator end() const;

	/// <summary>
	/// Iterator to reverse end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	reverse_iterator rend();

	/// <summary>
	/// Iterator to reverse end. Concurrency safe
	/// </summary>
	/// <returns>Iterator</returns>
	const_reverse_iterator rend() const;

	/// <summary>
	/// Items at key
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>Item count</returns>
	size_type count(Key key) const;

	/// <summary>
	/// Reserve space for at least this capacity
	/// </summary>
	/// <param name="newCapacity">Desired capacity</param>
	void reserve(size_type newCapacity);

	/// <summary>
	/// The relationship between the capacity and the bucket count
	/// </summary>
	/// <returns>capacity to bucket count ratio</returns>
	float max_load_factor() const noexcept;

	/// <summary>
	/// The relationship between the size and the bucket count
	/// </summary>
	/// <returns>size to bucket count ratio</returns>
	float load_factor() const noexcept;

	/// <summary>
	/// Attempt deletion of item
	/// Not concurrency safe
	/// </summary>
	/// <param name="key">Search key</param>
	/// <returns>True if item was present, and deleted</returns>
	bool unsafe_erase(Key key);

	/// <summary>
	/// Remove all items
	/// Not concurrency safe
	/// </summary>
	void unsafe_clear();

private:
	using packed_item_ptr = packed_ptr<item_type, chm_detail::item_flag>;

	struct bucket
	{
		packed_item_ptr item;
		std::size_t hash;
	};

	using bucket_array = shared_ptr<atomic_128<bucket>[]>;

	struct t_container
	{
		t_container() = default;
		t_container(bucket_array b)
			: buckets(std::move(b))
			, bucketCount(buckets.item_count())
		{
		}
		// For ownership
		bucket_array buckets;
		size_type bucketCount;
	};

	friend struct iterator;
	friend struct const_iterator;

	template <class ...Args>
	std::pair<iterator, bool> insert_internal(Args&& ... args);

	packed_item_ptr find_internal(Key key) const;

	bool refresh_tl(t_container& tl) const;

	void grow_buckets(t_container& tl);
	void reserve_internal(size_type desiredBucketCount);

	std::pair<iterator, bool> try_insert_in_bucket_array(bucket_array& buckets, item_type* item, std::size_t hash);

	typename packed_item_ptr scan_for_bucket(const item_type* from, int direction) const;

	static size_type find_bucket(const bucket_array& buckets, size_type bucketcount, std::size_t hash);
	static std::pair<size_type, bucket> find_slot_or_bucket(const bucket_array& buckets, size_type bucketcount, std::size_t hash);

	static void help_dispose_buckets(bucket_array& buckets);
	static void dispose_bucket(atomic_128<bucket>& b);

	static pool_allocator<item_type, true> create_pool_allocator(std::size_t initialCapacity, Allocator alloc);

	pool_allocator<item_type, true> m_itemAllocator;

	atomic_shared_ptr<atomic_128<bucket>[]> m_items;

	mutable tlm<t_container, allocator_type> t_items;

	std::atomic<size_type> m_size;

	allocator_type m_allocator;
};
template<class Key, class Value, class Hash, class Allocator>
inline concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map()
	: concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(chm_detail::Default_Capacity, Allocator())
{
}

template<class Key, class Value, class Hash, class Allocator>
inline concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(Allocator alloc)
	: concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(chm_detail::Default_Capacity, alloc)
{
}

template<class Key, class Value, class Hash, class Allocator>
inline concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(size_type capacity)
	: concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(capacity, Allocator())
{
}

template<class Key, class Value, class Hash, class Allocator>
inline concurrent_unordered_map<Key, Value, Hash, Allocator>::concurrent_unordered_map(size_type capacity, Allocator alloc)
	: m_itemAllocator(create_pool_allocator(align_value_prime(chm_detail::to_bucket_count(capacity)), alloc))
	, m_items(gdul::allocate_shared<atomic_128<bucket>[]>(align_value_prime(chm_detail::to_bucket_count(capacity)), alloc))
	, t_items(alloc, m_items.load(std::memory_order_relaxed))
	, m_size(0)
	, m_allocator(alloc)
{
}

template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_unordered_map<Key, Value, Hash, Allocator>::insert(const item_type& in)
{
	return insert_internal(in);
}

template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_unordered_map<Key, Value, Hash, Allocator>::insert(item_type&& in)
{
	return insert_internal(std::move(in));
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::find(Key key)
{
	packed_item_ptr item(find_internal(key));

	return iterator(this, item.ptr());
}

template<class Key, class Value, class Hash, class Allocator>
inline const typename concurrent_unordered_map<Key, Value, Hash, Allocator>::const_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::find(Key key) const
{
	const packed_item_ptr item(find_internal(key));

	return const_iterator(this, item.ptr());
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::value_type& concurrent_unordered_map<Key, Value, Hash, Allocator>::operator[](Key key)
{
	return at(key);
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::value_type& concurrent_unordered_map<Key, Value, Hash, Allocator>::at(Key key)
{
	iterator item(find(key));

	if (item != end()) {
		return item->second;
	}

	return insert(std::make_pair(Key(), Value())).first->second;
}
template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type concurrent_unordered_map<Key, Value, Hash, Allocator>::size() const
{
	return m_size.load(std::memory_order_relaxed);
}

template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_unordered_map<Key, Value, Hash, Allocator>::empty() const
{
	return !size();
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type concurrent_unordered_map<Key, Value, Hash, Allocator>::capacity() const
{
	return bucket_count() / chm_detail::Growth_Multiple;
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type concurrent_unordered_map<Key, Value, Hash, Allocator>::bucket_count() const noexcept
{
	return m_items.load(std::memory_order_acquire).item_count();
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::begin()
{
	return iterator(this, scan_for_bucket(nullptr, 1).ptr());
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::const_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::begin() const
{
	return const_iterator(this, scan_for_bucket(nullptr, 1).ptr());
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::reverse_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::rbegin()
{
	return reverse_iterator(iterator(this, scan_for_bucket(nullptr, -1).ptr()));
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::const_reverse_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::rbegin() const
{
	return reverse_iterator(const_iterator(this, scan_for_bucket(nullptr, -1).ptr()));
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::end()
{
	return iterator(this, nullptr);
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::const_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::end() const
{
	return const_iterator(this, nullptr);
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::reverse_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::rend()
{
	return reverse_iterator(end());
}

template<class Key, class Value, class Hash, class Allocator>
typename concurrent_unordered_map<Key, Value, Hash, Allocator>::const_reverse_iterator concurrent_unordered_map<Key, Value, Hash, Allocator>::rend() const
{
	return reverse_iterator(end());
}

template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_unordered_map<Key, Value, Hash, Allocator>::unsafe_erase(Key key)
{
	t_container& tl(t_items);

	refresh_tl(tl);

	const size_type index(find_bucket(tl.buckets, tl.bucketCount, hash_type()(key)));

	if (index == tl.bucketCount) {
		return false;
	}

	bucket& b(tl.buckets[index].my_val());

	if (!(b.item.extra() & chm_detail::item_flag_valid)) {
		return false;
	}

	std::uint8_t state(b.item.extra());
	state &= ~chm_detail::item_flag_valid;
	state |= chm_detail::item_flag_deleted;

	b.item.set_extra((chm_detail::item_flag)state);

	std::destroy_at(b.item.ptr());

	m_itemAllocator.deallocate(b.item.ptr());

	std::atomic_thread_fence(std::memory_order_release);

	return true;
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type concurrent_unordered_map<Key, Value, Hash, Allocator>::count(Key key) const
{
	return find(key) != end() ? 1 : 0;
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::reserve(size_type newCapacity)
{
	reserve_internal(chm_detail::to_bucket_count(newCapacity));
}

template<class Key, class Value, class Hash, class Allocator>
inline float concurrent_unordered_map<Key, Value, Hash, Allocator>::max_load_factor() const noexcept
{
	return 1.f / static_cast<float>(chm_detail::Growth_Multiple);
}

template<class Key, class Value, class Hash, class Allocator>
inline float concurrent_unordered_map<Key, Value, Hash, Allocator>::load_factor() const noexcept
{
	return static_cast<float>(size()) / static_cast<float>(capacity());
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::unsafe_clear()
{
	bucket_array buckets(m_items.load(std::memory_order_acquire));

	bucket emptyBucket;
	emptyBucket.hash = 0;
	emptyBucket.item = packed_item_ptr(nullptr);

	for (std::size_t i = 0; i < buckets.item_count(); ++i) {
		item_type* const item(buckets[i].my_val().item.ptr());

		if (item) {
			std::destroy_at(item);
			m_itemAllocator.deallocate(item);
		}

		buckets[i].my_val() = emptyBucket;
	}

	m_size.store(0, std::memory_order_relaxed);

	std::atomic_thread_fence(std::memory_order_release);
}

template<class Key, class Value, class Hash, class Allocator>
template<class ...Args>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_unordered_map<Key, Value, Hash, Allocator>::emplace(Args && ...args)
{
	return insert_internal(std::forward<Args>(args)...);
}

template<class Key, class Value, class Hash, class Allocator>
template<class ...Args>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_unordered_map<Key, Value, Hash, Allocator>::insert_internal(Args&& ... args)
{
	item_type* const item(m_itemAllocator.allocate());
	new (item) item_type(std::forward<Args>(args)...);

	const size_type hash(hash_type()(item->first));

	assert(hash && "Zero hash");

	for (;;) {
		t_container& tl(t_items);

		std::pair<iterator, bool> result(try_insert_in_bucket_array(tl.buckets, item, hash));

		if (result.first != end()) {

			if (result.second) {
				if (!((m_size++ * chm_detail::Growth_Multiple) < tl.bucketCount)) {
					grow_buckets(tl);
				}
			}
			else {
				std::destroy_at(item);
				m_itemAllocator.deallocate(item);
			}

			return result;
		}

		if (!refresh_tl(tl)) {
			grow_buckets(tl);
		}
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type concurrent_unordered_map<Key, Value, Hash, Allocator>::find_bucket(const bucket_array& buckets, size_type bucketCount, typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type hash)
{
	std::int8_t sign(1);
	for (size_type probe(0); probe < bucketCount; ++probe, sign *= -1) {
		const std::int64_t offset((probe * probe) * sign);

		const size_type index((hash + offset) % bucketCount);

		// Cheap peek at hash value
		const std::size_t thisHash(buckets[index].my_val().hash);

		if (thisHash == hash) {

			// If we find it, make sure we can see the whole bucket
			std::atomic_thread_fence(std::memory_order_acquire);

			return index;
		}
	}

	return bucketCount;
}

template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type, typename concurrent_unordered_map<Key, Value, Hash, Allocator>::bucket> concurrent_unordered_map<Key, Value, Hash, Allocator>::find_slot_or_bucket(const bucket_array& buckets, size_type bucketCount, typename concurrent_unordered_map<Key, Value, Hash, Allocator>::size_type hash)
{
	std::int8_t sign(1);
	for (size_type probe(0); probe < bucketCount; ++probe, sign *= -1) {
		const std::int64_t offset((probe * probe) * sign);

		const size_type index((hash + offset) % bucketCount);

		const bucket b(buckets[index].load());

		if (b.hash == hash || !b.hash) {
			return std::make_pair(index, b);
		}
	}

	return std::make_pair(bucketCount, bucket());
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::packed_item_ptr concurrent_unordered_map<Key, Value, Hash, Allocator>::scan_for_bucket(const item_type* from, int direction) const
{
	// Scan linearly for bucket. For use with iterators

	t_container& tl(t_items);

	refresh_tl(tl);

	size_type index(from ? find_bucket(tl.buckets, tl.bucketCount, hash_type()(from->first)) : (tl.bucketCount - 1 + direction) % tl.bucketCount);

	packed_item_ptr result(nullptr);

	for (index = index + direction; index < tl.bucketCount; index += direction) {
		const bucket at(tl.buckets[index].load());
		if (at.item.extra() & chm_detail::item_flag_valid) {
			result = at.item;
			break;
		}
	}

	return result;
}

template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_unordered_map<Key, Value, Hash, Allocator>::refresh_tl(typename concurrent_unordered_map<Key, Value, Hash, Allocator>::t_container& tl) const
{
	// Fast comparison
	if (tl.buckets == m_items) {
		return false;
	}

	tl.buckets = m_items.load(std::memory_order_acquire);
	tl.bucketCount = tl.buckets.item_count();

	return true;
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_unordered_map<Key, Value, Hash, Allocator>::packed_item_ptr concurrent_unordered_map<Key, Value, Hash, Allocator>::find_internal(Key key) const
{
	const std::size_t hash(hash_type()(key));
	t_container& tl(t_items);

	const std::size_t& bucketCount(tl.bucketCount);
	bucket_array& buckets(tl.buckets);

	do {
		const size_type index(find_bucket(buckets, bucketCount, hash));

		if (index != bucketCount) {
			const packed_item_ptr item(buckets[index].my_val().item);

			if (item.extra() & chm_detail::item_flag_valid) {
				return item;
			}
		}

	} while (refresh_tl(tl));

	return packed_item_ptr(nullptr);
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::grow_buckets(typename concurrent_unordered_map<Key, Value, Hash, Allocator>::t_container& tl)
{
	if (refresh_tl(tl)) {
		return;
	}

	reserve_internal(tl.bucketCount * 2);

	refresh_tl(tl);
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::reserve_internal(size_type desiredBucketCount)
{
	const size_type primeAlignedBucketCount(align_value_prime(desiredBucketCount));
	
	bucket_array expected(m_items.load(std::memory_order_relaxed));
	bucket_array desired;

	do {

		if (!(expected.item_count() < primeAlignedBucketCount)) {
			return;
		}

		help_dispose_buckets(expected);

		desired = gdul::allocate_shared<atomic_128<bucket>[]>(primeAlignedBucketCount, m_allocator);

		std::atomic_thread_fence(std::memory_order_acquire);

		for (size_type i = 0; i < expected.item_count(); ++i) {
			bucket b(expected[i].my_val());

			assert(b.item.extra() & chm_detail::item_flag_disposed && "No further items should exist in old bucket array, and all should be disposed");

			if (b.item) {
				std::pair<iterator, bool> [[maybe_unused]] itr(try_insert_in_bucket_array(desired, b.item.ptr(), hash_type()(b.item->first)));
				assert(itr.second && "Failed insert in new array");
			}
		}
	} while (!m_items.compare_exchange_strong(expected, desired, std::memory_order_release, std::memory_order_relaxed));
}

template<class Key, class Value, class Hash, class Allocator>
inline std::pair<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_unordered_map<Key, Value, Hash, Allocator>::try_insert_in_bucket_array(typename concurrent_unordered_map<Key, Value, Hash, Allocator>::bucket_array& buckets, typename concurrent_unordered_map<Key, Value, Hash, Allocator>::item_type* item, std::size_t hash)
{
	// Our break conditions are:
	// * A slot or existing hash is not found 
	// * The item has been disposed (that is, it is flagged to be moved to a larger array)
	// * The item was previously inserted
	// * The item was inserted successfully
	for (;;) {
		const std::pair<size_type, bucket> findResult(find_slot_or_bucket(buckets, buckets.item_count(), hash));
		const std::size_t index(findResult.first);

		if (findResult.first == buckets.item_count()) {
			return std::make_pair(end(), false);
		}

		bucket expected(findResult.second);

		if (expected.item.extra() & chm_detail::item_flag_disposed) {
			return std::make_pair(end(), false);
		}

		if (expected.item.extra() & chm_detail::item_flag_valid) {
			return std::make_pair(iterator(this, expected.item.ptr()), false);
		}

		bucket desired;
		desired.hash = hash;
		desired.item.set_ptr(item);
		desired.item.set_extra(chm_detail::item_flag_valid);

		if (buckets[index].compare_exchange_strong(expected, desired)) {
			return std::make_pair(iterator(this, item), true);
		}
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::help_dispose_buckets(typename concurrent_unordered_map<Key, Value, Hash, Allocator>::bucket_array& buckets)
{
	for (size_type i = 0; i < buckets.item_count(); ++i) {
		dispose_bucket(buckets[i]);
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline void concurrent_unordered_map<Key, Value, Hash, Allocator>::dispose_bucket(atomic_128<bucket>& b)
{
	bucket expected(b.my_val());

	while (!(expected.item.extra() & chm_detail::item_flag_disposed)) {
		bucket desired(expected);
		desired.item.set_extra((chm_detail::item_flag)(desired.item.extra() | chm_detail::item_flag_disposed));

		if (b.compare_exchange_strong(expected, desired)) {
			break;
		}
	}
}

template<class Key, class Value, class Hash, class Allocator>
inline pool_allocator<typename concurrent_unordered_map<Key, Value, Hash, Allocator>::item_type, true> concurrent_unordered_map<Key, Value, Hash, Allocator>::create_pool_allocator(std::size_t initialCapacity, Allocator alloc)
{
	memory_pool pool;
	pool.init<sizeof(item_type), alignof(item_type), Allocator>(initialCapacity, 1, alloc);
	return pool.create_allocator<item_type, true>();
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
		m_at = m_parent->scan_for_bucket(m_at, 1).ptr();
		return *this;
	}
	iterator operator--()
	{
		m_at = m_parent->scan_for_bucket(m_at, -1).ptr();
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
		return *m_at;
	}

	const typename iterator_base<Map>::value_type* operator->() const
	{
		return m_at;
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
		m_at = m_parent->scan_for_bucket(m_at, 1).ptr();
		return *this;
	}
	const_iterator operator--() const
	{
		m_at = m_parent->scan_for_bucket(m_at, -1).ptr();
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
