#pragma once


#include <xhash>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/thread_local_member/thread_local_member.h>
#include <gdul/atomic_128/atomic_128.h>

namespace gdul {

namespace chm_detail {
using size_type = std::size_t;

constexpr size_type Default_Capacity = 32;

template <class Key, class Value>
struct bucket;

constexpr size_type to_bucket_count(size_type desiredCapacity)
{
	// Something like 1.4 times desired.. 
	return desiredCapacity;
}

template <class Item>
struct iterator;
template <class Item>
struct const_iterator;
}

template <class Key, class Value, class Hash = std::hash<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_hash_map
{
public:
	using size_type = typename chm_detail::size_type;
	using key_type = Key;
	using value_type = Value;
	using hash_type = Hash;
	using allocator_type = Allocator;
	using bucket_type = chm_detail::bucket<key_type, value_type>;
	using item_type = std::pair<key_type, value_type>;
	using iterator = chm_detail::iterator<item_type>;
	using const_iterator = chm_detail::const_iterator<const item_type>;

	concurrent_hash_map();
	concurrent_hash_map(Allocator);
	concurrent_hash_map(size_type capacity);
	concurrent_hash_map(size_type capacity, Allocator);

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
	/// <returns>At bucket with matching key on success. end on failure</returns>
	iterator find(Key k);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>At bucket with matching key on success. end on failure</returns>
	const const_iterator find(Key k) const;

	iterator end() { return iterator(nullptr); }
	const_iterator end() const { return const_iterator(nullptr); }

private:
	using bucket_items_type = typename bucket_type::items;
	using bucket_array = shared_ptr<bucket_type[]>;

	template <class In>
	std::pair<iterator, bool> insert_internal(In&& in);

	size_type find_index(const bucket_array& buckets, size_type hash) const;

	static bool try_store_to_bucket(item_type* item, std::uint64_t hash, bucket_type& bucket, bucket_items_type& expectedBucketContents);

	bool refresh_tl() const;

	concurrent_guard_pool<item_type, allocator_type> m_pool;
	atomic_shared_ptr<bucket_type[]> m_items;
	mutable tlm<bucket_array, allocator_type> t_items;
	allocator_type m_allocator;
};
namespace chm_detail {
template <class Key, class Value>
struct bucket
{
	struct items
	{
		std::uint64_t hash = 0;
		std::pair<Key, Value>* kv = nullptr;
	};
	atomic_128<items> m_storage;
};
}
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
	: m_pool((std::uint32_t)chm_detail::to_bucket_count(capacity), (std::uint32_t)chm_detail::to_bucket_count(capacity) / std::thread::hardware_concurrency(), alloc)
	, m_items(gdul::allocate_shared<bucket_type[]>(chm_detail::to_bucket_count(capacity), alloc))
	, t_items(m_items.load(), alloc)
	, m_allocator(alloc)
{
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

	do {
		bucket_array& tBuckets(t_items);
		const size_type index(find_index(tBuckets, hash));

		if (index != tBuckets.item_count()) {

			std::atomic_thread_fence(std::memory_order_acquire);
			
			return iterator(tBuckets[index].m_storage.my_val().kv);
		}

	} while (refresh_tl());

	return end();
}
template<class Key, class Value, class Hash, class Allocator>
inline const typename concurrent_hash_map<Key, Value, Hash, Allocator>::const_iterator concurrent_hash_map<Key, Value, Hash, Allocator>::find(Key k) const
{
	const std::uint64_t hash(hash_type()(k));

	do {
		const bucket_array& tBuckets(t_items);
		const size_type index(find_index(tBuckets, hash));

		if (index != tBuckets.item_count()) {

			std::atomic_thread_fence(std::memory_order_acquire);

			return const_iterator(tBuckets[index].m_storage.my_val().kv);
		}

	} while (refresh_tl());

	return end();
}
template<class Key, class Value, class Hash, class Allocator>
template<class In>
inline std::pair<typename concurrent_hash_map<Key, Value, Hash, Allocator>::iterator, bool> concurrent_hash_map<Key, Value, Hash, Allocator>::insert_internal(In&& in)
{
	const std::uint64_t hash(hash_type()(in.first));

	bucket_array& tBuckets(t_items);

	const size_type bucketCount(tBuckets.item_count());
	const size_type nativeIndex(hash % bucketCount);

	item_type* const item(m_pool.get());
	*item = std::forward<In>(in);

	for (size_type probe(0); probe < bucketCount; ++probe) {

		const size_type index(nativeIndex + (probe * probe) % bucketCount);

		bucket_type& bucket(tBuckets[index]);
		typename bucket_type::items bucketItems(bucket.m_storage.my_val());

		if (bucketItems.hash == hash) {
			return std::make_pair(iterator(bucketItems.kv), false);
		}

		if (bucketItems.hash) {
			continue;
		}

		if (try_store_to_bucket(item, hash, bucket, bucketItems)) {
			return std::make_pair(iterator(item), true);
		}

		if (bucketItems.hash == hash) {
			return std::make_pair(iterator(bucketItems.kv), false);
		}
	}

	return std::make_pair(end(), false);
}

template<class Key, class Value, class Hash, class Allocator>
inline typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type concurrent_hash_map<Key, Value, Hash, Allocator>::find_index(const typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_array& buckets, typename concurrent_hash_map<Key, Value, Hash, Allocator>::size_type hash) const
{
	const size_type bucketCount(buckets.item_count());
	const size_type nativeIndex(hash % bucketCount);

	for (size_type probe(0); probe < bucketCount; ++probe) {
		const size_type index(nativeIndex + (probe * probe) % bucketCount);

		const bucket_type& bucket(buckets[index]);
		const typename bucket_type::items& bucketItems(bucket.m_storage.my_val());

		if (bucketItems.hash == hash) {
			return index;
		}

		if (!bucketItems.hash) {
			break;
		}
	}

	return bucketCount;
}
template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_hash_map<Key, Value, Hash, Allocator>::try_store_to_bucket(typename concurrent_hash_map<Key, Value, Hash, Allocator>::item_type* item, std::uint64_t hash, typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_type& bucket, typename concurrent_hash_map<Key, Value, Hash, Allocator>::bucket_items_type& expectedBucketContents)
{
	bucket_items_type desiredItems;
	desiredItems.hash = hash;
	desiredItems.kv = item;

	const bool result(bucket.m_storage.compare_exchange_strong(expectedBucketContents, desiredItems));

	if (!result && !expectedBucketContents.kv) {
		return bucket.m_storage.compare_exchange_strong(expectedBucketContents, desiredItems);
	}

	return result;
}
template<class Key, class Value, class Hash, class Allocator>
inline bool concurrent_hash_map<Key, Value, Hash, Allocator>::refresh_tl() const
{
	if (t_items == m_items) {
		return false;
	}

	t_items = m_items.load(std::memory_order_acquire);

	return true;
}
namespace chm_detail {
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
		return this->m_at;
	}

	std::pair<iterator_base<Item>::key_type, iterator_base<Item>::value_type>* operator->()
	{
		return &this->m_at;
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