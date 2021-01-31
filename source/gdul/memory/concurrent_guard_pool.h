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

#include <gdul/containers/concurrent_queue.h>
#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/memory/thread_local_member.h>
#include <gdul/math/math.h>

#include <assert.h>
#include <thread>
#include <vector>
#include <thread>
#include <atomic>
#include <stdint.h>
#include <limits>
#include <algorithm>
#include <cmath>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

namespace gdul {

namespace cgp_detail {

using size_type = std::size_t;
using defaul_allocator = std::allocator<std::uint8_t>;

constexpr size_type Defaul_Block_Size = 16;
constexpr size_type Defaul_Tl_Cache_Size = 4;
constexpr size_type Max_Users = 16;
constexpr std::uint8_t Capacity_Bits = 19;

struct critical_sec;

struct alignas(std::hardware_destructive_interference_size) user_index { size_type i; };

}
/// <summary>
/// Concurrency safe, lock free object pool with an additional method of ensuring
/// safe memory reclamation of shared-access objects.
/// </summary>
template <class T, class Allocator = cgp_detail::defaul_allocator>
class concurrent_guard_pool
{
public:
	using size_type = typename cgp_detail::size_type;
	using allocator_type = Allocator;

	constexpr static size_type Max_Capacity = size_type(std::numeric_limits<std::uint16_t>::max() << 3) | size_type(1 | 2 | 4);

	/// <summary>
	/// constructor
	/// </summary>
	concurrent_guard_pool();
	/// <summary>
	/// constructor
	/// </summary>
	/// <param name="allocator">allocator to use</param>
	concurrent_guard_pool(Allocator allocator);
	/// <summary>
	/// constructor
	/// </summary>
	/// <param name="baseCapacity">initial global cached items</param>
	/// <param name="tlCacheSize">size of the thread local caches</param>
	/// <param name="rowLength">number of items returned by get</param>
	concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize, size_type rowLength);

	/// <summary>
	/// constructor
	/// </summary>
	/// <param name="baseCapacity">initial global cached items</param>
	/// <param name="tlCacheSize">size of the thread local caches</param>
	concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize);

	/// <summary>
	/// constructor
	/// </summary>
	/// <param name="baseCapacity">initial global cached items</param>
	/// <param name="tlCacheSize">size of the thread local caches</param>
	/// <param name="allocator">allocator to use</param>
	concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize, Allocator allocator);

	/// <summary>
	/// constructor
	/// </summary>
	/// <param name="baseCapacity">initial global cached items</param>
	/// <param name="tlCacheSize">size of the thread local caches</param>
	/// <param name="rowLength">number of items returned by get</param>
	/// <param name="allocator">allocator to use</param>
	concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize, size_type  rowLength, Allocator allocator);

	/// <summary>
	/// destructor
	/// </summary>
	~concurrent_guard_pool();

	/// <summary>
	/// Fetch item
	/// </summary>
	/// <returns>Unused item</returns>
	T* get();

	/// <summary>
	/// Return item for reuse
	/// </summary>
	/// <param name="item">item to reuse</param>
	void recycle(T* item);

	/// <summary>
	/// This method may be used to wrap critical calls if items are used in a shared-memory setting to
	/// ensure no item that is potentially referenced by another thread is recycled.
	/// Ex. auto result = guard(&node_tree::remove_from_tree, &m_tree, key);
	/// </summary>
	/// <param name="f">critical section callable to guard</param>
	/// <param name="args">arguments for passed callable</param>
	/// <returns>return value of passed callable</returns>
	template <class Fn, class ...Args>
	std::invoke_result_t<Fn, Args...> guard(Fn f, Args&&... args);

	/// <summary>
	/// Reset structure to initial state. Assumes no concurrent access
	/// </summary>
	inline void unsafe_reset();

private:
	struct block_node
	{
		atomic_shared_ptr<T[]> m_items;

		std::uintptr_t m_blockKey = 0;

		std::atomic<size_type> m_pushSync = 0;
		std::atomic<size_type> m_livingItems = Max_Capacity;
	};

	using tl_cache = shared_ptr<T* []>;

	struct tl_container
	{
		tl_container() = default;
		tl_container(const shared_ptr<tlm_detail::index_pool<void>>& ip, Allocator alloc)
		{
			m_indexPool = ip;
			m_userIndex = m_indexPool->get(alloc);
		}
		~tl_container()
		{
			if (m_indexPool) {
				m_indexPool->add((std::uint32_t)m_userIndex);
			}
		}
		tl_cache m_cache;
		tl_cache m_deferredReclaimCache;

		size_type m_cacheIndex = Max_Capacity;
		size_type m_reclaimCacheIndex = Max_Capacity;

		size_type m_userIndex = std::numeric_limits<size_type>::max();

		std::array<size_type, cgp_detail::Max_Users> m_indexCache{};

		std::vector<std::pair<size_type, tl_cache>> m_deferredReclaimCaches;

		shared_ptr<tlm_detail::index_pool<void>> m_indexPool;
	};

	void add_to_tl_deferred_reclaim_cache(T* item, tl_container& tl);

	void evaluate_caches_for_reclamation(tl_container& tl);

	tl_cache acquire_tl_cache_full();
	tl_cache acquire_tl_cache_empty();

	bool fetch_from_tl_cache(tl_container& tl, T*& outItem);

	size_type fetch_user_index(const tl_container& tl) const;

	std::pair<size_type, size_type> update_index_cache(tl_container& tl);

	static bool is_contained_within(const T* item, std::uintptr_t blockKey);
	static size_type to_capacity(std::uintptr_t blockKey);
	static const T* to_begin(std::uintptr_t blockKey);
	static std::uintptr_t to_block_key(const T* begin, size_type capacity);

	bool is_up_to_date(const T* item) const;
	void discard_item(const T* item);

	void try_alloc_block(std::uint8_t blockIndex);

	alignas(std::hardware_destructive_interference_size) concurrent_queue<tl_cache, Allocator> m_fullCaches;
	alignas(std::hardware_destructive_interference_size) concurrent_queue<tl_cache, Allocator> m_emptyCaches;
	alignas(std::hardware_destructive_interference_size) std::array<cgp_detail::user_index, cgp_detail::Max_Users> m_indices;

	block_node m_blocks[cgp_detail::Capacity_Bits];

	// Shameless reuse of index pool from tlm
	shared_ptr<tlm_detail::index_pool<void>> m_indexPool;

	tlm<tl_container, Allocator> t_tlContainer;

	size_type m_tlCacheSize;
	size_type m_rowLength;
	Allocator m_allocator;

	std::atomic<std::uint8_t> m_blocksEndIndex;
};

template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool()
	: concurrent_guard_pool(cgp_detail::Defaul_Block_Size, cgp_detail::Defaul_Tl_Cache_Size, Allocator())
{
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool(Allocator allocator)
	: concurrent_guard_pool(cgp_detail::Defaul_Block_Size, cgp_detail::Defaul_Tl_Cache_Size, allocator)
{
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool(typename concurrent_guard_pool<T, Allocator>::size_type baseCapacity, size_type  rowLength, size_type tlCacheSize)
	: concurrent_guard_pool(baseCapacity, tlCacheSize, rowLength, Allocator())
{
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize)
	: concurrent_guard_pool(baseCapacity, tlCacheSize, 1, Allocator())
{
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize, Allocator allocator)
	: concurrent_guard_pool(baseCapacity, tlCacheSize, 1, allocator)
{
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::concurrent_guard_pool(size_type baseCapacity, size_type tlCacheSize, size_type  rowLength, Allocator allocator)
	: m_fullCaches(allocator)
	, m_emptyCaches(allocator)
	, m_indices{}
	, m_blocks{}
	, m_indexPool(gdul::allocate_shared<tlm_detail::index_pool<void>>(allocator))
	, t_tlContainer(allocator, m_indexPool, allocator)
	, m_tlCacheSize(0)
	, m_rowLength(rowLength)
	, m_allocator()
	, m_blocksEndIndex(0)
{
	assert(baseCapacity && tlCacheSize && "Cannot instantiate pool with zero sizes");

	for (std::uint8_t i = 0; i < cgp_detail::Capacity_Bits; ++i)
		m_blocks[i].m_livingItems.store((size_type)std::pow(2.f, (float)(i + 1)), std::memory_order_relaxed);

	const size_type alignedSize(align_value_pow2(baseCapacity, Max_Capacity));
	m_tlCacheSize = (size_type)align_value_pow2(tlCacheSize, alignedSize);

	const std::uint8_t blockIndex((std::uint8_t)std::log2f((float)alignedSize) - 1);

	m_blocksEndIndex.store(blockIndex, std::memory_order_relaxed);

	try_alloc_block(blockIndex);
}
template<class T, class Allocator>
inline concurrent_guard_pool<T, Allocator>::~concurrent_guard_pool()
{
	unsafe_reset();
}

template<class T, class Allocator>
inline T* concurrent_guard_pool<T, Allocator>::get()
{
	T* ret;
	
	tl_container& tl(t_tlContainer);

	while (!fetch_from_tl_cache(tl, ret)) {

		if (tl.m_cache) {
			m_emptyCaches.push(std::move(tl.m_cache));
		}

		tl.m_cache = acquire_tl_cache_full();
		tl.m_cacheIndex = 0;
	}

	return ret;
}

template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::recycle(T* item)
{
	if (is_up_to_date(item)) {
		tl_container& tl(t_tlContainer);
		add_to_tl_deferred_reclaim_cache(item, tl);
	}
	else {
		discard_item(item);
	}
}

template<class T, class Allocator>
template<class Fn, class ...Args>
inline std::invoke_result_t<Fn, Args...> concurrent_guard_pool<T, Allocator>::guard(Fn f, Args && ...args)
{
	tl_container& tl(t_tlContainer);

	const size_type userIndex(fetch_user_index(tl));
	const cgp_detail::critical_sec cs(m_indices[userIndex].i);

	return std::invoke(f, std::forward<Args>(args)...);
}

template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::unsafe_reset()
{
	const std::uint8_t blockCount(m_blocksEndIndex.exchange(0, std::memory_order_relaxed));
	for (std::uint8_t i = 0; i < blockCount; ++i) {
		m_blocks[i].m_blockKey = 0;
		m_blocks[i].m_items = shared_ptr<T[]>(nullptr);
		m_blocks[i].m_items.unsafe_set_version(0);
		m_blocks[i].m_pushSync = 0;
		m_blocks[i].m_livingItems.store((size_type)std::pow(2.f, (float)(i + 1)));
	}

	m_emptyCaches.unsafe_reset();
	m_fullCaches.unsafe_reset();
}

template<class T, class Allocator>
inline bool concurrent_guard_pool<T, Allocator>::is_up_to_date(const T* item) const
{
	assert(m_blocksEndIndex.load() != 0 && "Expected to find value, is pool initialized?");

	const size_type blocksEnd(m_blocksEndIndex.load(std::memory_order_relaxed));
	const size_type lastBlock(blocksEnd - 1);
	const size_type nextBlock(lastBlock + 1); // Speculative, in case this item was allocated really recently

	if (is_contained_within(item, m_blocks[lastBlock].m_blockKey)) {
		return true;
	}
	if (is_contained_within(item, m_blocks[nextBlock].m_blockKey) && blocksEnd < cgp_detail::Capacity_Bits) {
		return true;
	}
	return false;
}
template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::add_to_tl_deferred_reclaim_cache(T* item, tl_container& tl)
{
	size_type reclaimIndex(tl.m_reclaimCacheIndex++);

	if (!(reclaimIndex < tl.m_deferredReclaimCache.item_count())) {

		evaluate_caches_for_reclamation(tl);

		tl.m_deferredReclaimCache = acquire_tl_cache_empty();
		tl.m_reclaimCacheIndex = 1;
		reclaimIndex = 0;
	}

	tl.m_deferredReclaimCache[reclaimIndex] = item;
}
template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::evaluate_caches_for_reclamation(tl_container& tl)
{
	const std::pair<size_type, size_type> accessMasks(update_index_cache(tl));

	std::vector<std::pair<size_type, tl_cache>>& caches(tl.m_deferredReclaimCaches);

	std::for_each(caches.begin(), caches.end(), [&accessMasks](auto& elem) {elem.first &= (accessMasks.second & accessMasks.first); });

	if (tl.m_deferredReclaimCache) {
		caches.push_back(std::make_pair(accessMasks.first, std::move(tl.m_deferredReclaimCache)));
	}

	for (std::size_t i = 0; i < caches.size(); ++i) {
		if (!caches[i].first) {
			m_fullCaches.push(std::move(caches[i].second));
			caches[i] = std::move(caches.back());
			caches.pop_back();
		}
	}
}
template<class T, class Allocator>
inline typename concurrent_guard_pool<T, Allocator>::tl_cache concurrent_guard_pool<T, Allocator>::acquire_tl_cache_full()
{
	tl_cache ret(nullptr);

	while (!m_fullCaches.try_pop(ret)) {
		const std::uint8_t endIndex(m_blocksEndIndex.load(std::memory_order_acquire));

		if (!m_fullCaches.try_pop(ret)) {
			try_alloc_block(endIndex);
		}
	}

	return ret;
}
template<class T, class Allocator>
inline typename concurrent_guard_pool<T, Allocator>::tl_cache concurrent_guard_pool<T, Allocator>::acquire_tl_cache_empty()
{
	tl_cache result;

	if (!m_emptyCaches.try_pop(result)) {
		result = gdul::allocate_shared<T* []>(m_tlCacheSize, m_allocator);
	}

	return result;
}
template<class T, class Allocator>
inline bool concurrent_guard_pool<T, Allocator>::fetch_from_tl_cache(tl_container& tl, T*& outItem)
{
	const size_type index(tl.m_cacheIndex++);

	if (!(index < tl.m_cache.item_count())) {
		return false;
	}

	outItem = tl.m_cache[index];

	return true;
}
template<class T, class Allocator>
inline typename concurrent_guard_pool<T, Allocator>::size_type concurrent_guard_pool<T, Allocator>::fetch_user_index(const tl_container& tl) const
{
	const size_type userIndex(tl.m_userIndex);

	assert(userIndex < cgp_detail::Max_Users && "Too many active threads accessing this structure");

	return userIndex;
}
template<class T, class Allocator>
inline std::pair<typename concurrent_guard_pool<T, Allocator>::size_type, typename concurrent_guard_pool<T, Allocator>::size_type> concurrent_guard_pool<T, Allocator>::update_index_cache(tl_container& tl)
{
	std::pair<size_type, size_type> masks{};

	for (size_type i = 0; i < m_indices.size(); ++i) {
		const size_type previous(tl.m_indexCache[i]);
		const size_type current(m_indices[i].i);
		const size_type even(current % 2);
		const size_type changed(previous == current);
		const size_type evenBit(even << i);
		const size_type changeBit(changed << i);

		masks.first |= evenBit;
		masks.second |= changeBit;

		tl.m_indexCache[i] = current;
	}

	const size_type userIndex(fetch_user_index(tl));
	const size_type ownBit(size_type(1) << userIndex);

	masks.first &= ~ownBit;
	masks.second &= ~ownBit;

	return masks;
}
template<class T, class Allocator>
inline bool concurrent_guard_pool<T, Allocator>::is_contained_within(const T* item, std::uintptr_t blockKey)
{
	const T* const begin(to_begin(blockKey));

	return !(item < begin) && (item < (begin + to_capacity(blockKey)));
}
template<class T, class Allocator>
inline typename concurrent_guard_pool<T, Allocator>::size_type concurrent_guard_pool<T, Allocator>::to_capacity(std::uintptr_t blockKey)
{
	constexpr std::uintptr_t toShift(64 - cgp_detail::Capacity_Bits);
	const std::uintptr_t capacity(blockKey >> toShift);

	return (size_type)capacity;
}
template<class T, class Allocator>
inline const T* concurrent_guard_pool<T, Allocator>::to_begin(std::uintptr_t blockKey)
{
	const std::uintptr_t noCapacity(blockKey << cgp_detail::Capacity_Bits);
	const std::uintptr_t ptrBlock(noCapacity >> 16);

	return (const T*)ptrBlock;
}
template<class T, class Allocator>
inline std::uintptr_t concurrent_guard_pool<T, Allocator>::to_block_key(const T* begin, size_type capacity)
{
	constexpr std::uintptr_t toShift((sizeof(std::uintptr_t) * 8) - cgp_detail::Capacity_Bits);
	const std::uintptr_t capacityBase(capacity);
	const std::uintptr_t capacityShift(capacityBase << toShift);
	const std::uintptr_t ptrBase((std::uintptr_t)begin);
	const std::uintptr_t ptrShift(ptrBase >> 3);
	const std::uintptr_t blockKey(ptrShift | capacityShift);

	return blockKey;
}
template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::discard_item(const T* item)
{
	const std::uint8_t blockCount(m_blocksEndIndex.load(std::memory_order_acquire));

	for (std::uint8_t i = 0; i < blockCount; ++i) {
		if (is_contained_within(item, m_blocks[i].m_blockKey)) {
			if (m_blocks[i].m_livingItems.fetch_sub(1, std::memory_order_relaxed) == 1) {
				m_blocks[i].m_items.store(gdul::shared_ptr<T[]>(nullptr));
			}
			return;
		}
	}
	assert(false && "Item does not belong in this structure");
}
template<class T, class Allocator>
inline void concurrent_guard_pool<T, Allocator>::try_alloc_block(std::uint8_t blockIndex)
{
	assert((blockIndex < cgp_detail::Capacity_Bits) && "Capacity overflow");

	if (!(blockIndex < cgp_detail::Capacity_Bits))
		return;

	shared_ptr<T[]> expected(m_blocks[blockIndex].m_items.load(std::memory_order_relaxed));

	if (expected.get_version() == 2)
		return;

	const size_type size((size_type)std::powf(2.f, (float)blockIndex + 1));

	if (expected.get_version() == 0) {

		shared_ptr<T[]> desired(gdul::allocate_shared<T[]>(size * m_rowLength, m_allocator));
		if (m_blocks[blockIndex].m_items.compare_exchange_strong(expected, desired, std::memory_order_release)) {
			expected = std::move(desired);
		}
	}

	const std::uintptr_t blockKey(to_block_key(expected.get(), (size_type)expected.item_count()));
	m_blocks[blockIndex].m_blockKey = blockKey;

	// Divide into tl caches & add to cachepool
	size_type pushIndex(m_blocks[blockIndex].m_pushSync.fetch_add(m_tlCacheSize, std::memory_order_relaxed));
	while (pushIndex < size) {
		tl_cache cache(gdul::allocate_shared<T* []>(m_tlCacheSize, m_allocator));
		for (size_type i = pushIndex; i < (pushIndex + m_tlCacheSize); ++i) {
			cache[i - pushIndex] = &expected[i * m_rowLength];
		}
		m_fullCaches.push(std::move(cache));
		pushIndex = m_blocks[blockIndex].m_pushSync.fetch_add(m_tlCacheSize, std::memory_order_relaxed);
	}

	m_blocksEndIndex.compare_exchange_strong(blockIndex, blockIndex + 1, std::memory_order_release);
}
namespace cgp_detail {

struct critical_sec
{
	critical_sec(size_type& tlIndex) : m_tlIndex(++tlIndex) {}
	~critical_sec() { ++m_tlIndex; }

	size_type& m_tlIndex;
};
}
}
#pragma warning(pop)