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
#include <gdul/math/math.h>

#include <assert.h>

namespace gdul {

namespace cop_detail {
constexpr std::size_t DefaulBlockSize = 8;
}

// Concurrency safe, lock free object pool. Contains an optimization where recycled objects belonging to an 'old' 
// block will be discarded. This will make the objects retain good cache locality as the structure ages. 
// Block capacity grows over time.
template <class Object, class Allocator = std::allocator<std::uint8_t>>
class concurrent_object_pool
{
public:
	concurrent_object_pool();
	concurrent_object_pool(Allocator allocator);
	concurrent_object_pool(std::size_t baseCapacity);
	concurrent_object_pool(std::size_t baseCapacity, Allocator allocator);
	~concurrent_object_pool();

	constexpr static std::size_t MaxCapacity = std::numeric_limits<std::uint16_t>::max() * 2 * 2 * 2;

	inline Object* get();

	inline void recycle(Object* object);

	inline std::size_t avaliable() const;

	inline void unsafe_reset();

private:
	constexpr static std::size_t CapacityBits = 19;

	struct block_node
	{
		atomic_shared_ptr<Object[]> m_objects;

		std::uintptr_t m_blockKey = 0;

		std::atomic<std::uint32_t> m_pushSync = 0;
		std::atomic<std::uint32_t> m_livingObjects = MaxCapacity;
	};

	static bool is_contained_within(const Object* obj, std::uintptr_t blockKey);
	static std::uint32_t to_capacity(std::uintptr_t blockKey);
	static const Object* to_begin(std::uintptr_t blockKey);
	static const Object* to_end(std::uintptr_t blockKey);
	static std::uintptr_t to_block_key(const Object* begin, std::uint32_t capacity);
	static std::uintptr_t get_block_key_from(const block_node& from);

	bool is_obsolete_hint(const Object* obj) const;
	bool is_obsolete(const Object* obj) const;
	void discard_object(const Object* obj);
	void force_hint_update();
	void try_alloc_block(std::uint8_t blockIndex);

	block_node m_blocks[CapacityBits];

	std::atomic<std::uintptr_t> m_activeBlockKeyHint;

	concurrent_queue<Object*, Allocator> m_unusedObjects;

	Allocator m_allocator;

	std::atomic<std::uint8_t> m_blocksEndIndex;
};
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool()
	: concurrent_object_pool(cop_detail::DefaulBlockSize, Allocator())
{}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(Allocator allocator)
	: concurrent_object_pool(cop_detail::DefaulBlockSize, allocator)
{}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t baseCapacity)
	: concurrent_object_pool(baseCapacity, Allocator())
{}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t baseCapacity, Allocator allocator)
	: m_blocks{}
	, m_activeBlockKeyHint(0)
	, m_unusedObjects(allocator)
	, m_allocator()
	, m_blocksEndIndex(0)
{
	for (std::uint8_t i = 0; i < CapacityBits; ++i)
		m_blocks[i].m_livingObjects.store((std::uint32_t)std::pow(2.f, (float)(i + 1)), std::memory_order_relaxed);

	const std::size_t alignedSize(align_value_pow2(baseCapacity, MaxCapacity));
	const std::uint8_t blockIndex((std::uint8_t)std::log2f((float)alignedSize) - 1);

	m_blocksEndIndex.store(blockIndex, std::memory_order_relaxed);

	try_alloc_block(blockIndex);
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::~concurrent_object_pool()
{
	unsafe_reset();
}
template<class Object, class Allocator>
inline Object* concurrent_object_pool<Object, Allocator>::get()
{
	Object* out;

	while (!m_unusedObjects.try_pop(out)) {

		const std::uint8_t endIndex(m_blocksEndIndex.load(std::memory_order_acquire));
		if (!m_unusedObjects.try_pop(out)) {
			try_alloc_block(endIndex);
		}
	}
	return out;
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::recycle(Object* object)
{
	if (!is_obsolete_hint(object) || !is_obsolete(object)) 
		m_unusedObjects.push(object);
	else
		discard_object(object);
}
template<class Object, class Allocator>
inline std::size_t concurrent_object_pool<Object, Allocator>::avaliable() const
{
	return m_unusedObjects.size();
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::unsafe_reset()
{
	m_activeBlockKeyHint.store(0, std::memory_order_relaxed);

	const std::uint8_t blockCount(m_blocksEndIndex.exchange(0, std::memory_order_relaxed));
	for (std::uint8_t i = 0; i < blockCount; ++i){
		m_blocks[i].~block_node();
		new (&m_blocks[i]) block_node();

		m_blocks[i].m_livingObjects.store((std::uint32_t)std::pow(2.f, (float)(i + 1)));
	}

	m_unusedObjects.unsafe_reset();
}
template<class Object, class Allocator>
inline bool concurrent_object_pool<Object, Allocator>::is_obsolete_hint(const Object* obj) const
{
	return !is_contained_within(obj, m_activeBlockKeyHint.load(std::memory_order_relaxed));
}
template<class Object, class Allocator>
inline bool concurrent_object_pool<Object, Allocator>::is_obsolete(const Object* obj) const
{
	const std::uint8_t blockEndIndex(m_blocksEndIndex.load());
	const std::uint8_t blockLastIndex(blockEndIndex - 1);

	const bool containedWithinNext(blockEndIndex == CapacityBits ? false : is_contained_within(obj, m_blocks[blockEndIndex].m_blockKey));
	const bool containedWithinLast(is_contained_within(obj, get_block_key_from(m_blocks[blockLastIndex])));

	return !(containedWithinLast | containedWithinNext);
}
template<class Object, class Allocator>
inline bool concurrent_object_pool<Object, Allocator>::is_contained_within(const Object* obj, std::uintptr_t blockKey)
{
	const Object* const begin(to_begin(blockKey));

	return (!(obj < begin)) & (obj < (begin + to_capacity(blockKey)));
}
template<class Object, class Allocator>
inline std::uint32_t concurrent_object_pool<Object, Allocator>::to_capacity(std::uintptr_t blockKey)
{
	constexpr std::uintptr_t toShift(64 - CapacityBits);
	const std::uintptr_t capacity(blockKey >> toShift);

	return (std::uint32_t)capacity;
}
template<class Object, class Allocator>
inline const Object* concurrent_object_pool<Object, Allocator>::to_begin(std::uintptr_t blockKey)
{
	const std::uintptr_t noCapacity(blockKey << CapacityBits);
	const std::uintptr_t ptrBlock(noCapacity >> 16);

	return (const Object*)ptrBlock;
}
template<class Object, class Allocator>
inline const Object* concurrent_object_pool<Object, Allocator>::to_end(std::uintptr_t blockKey)
{
	return to_begin(blockKey) + to_capacity(blockKey);
}
template<class Object, class Allocator>
inline std::uintptr_t concurrent_object_pool<Object, Allocator>::to_block_key(const Object* begin, std::uint32_t capacity)
{
	constexpr std::uintptr_t toShift(64 - CapacityBits);
	const std::uintptr_t capacityBase(capacity);
	const std::uintptr_t capacityShift(capacityBase << toShift);
	const std::uintptr_t ptrBase((std::uintptr_t)begin);
	const std::uintptr_t ptrShift(ptrBase >> 3);
	const std::uintptr_t blockKey(ptrShift | capacityShift);

	return blockKey;
}
template<class Object, class Allocator>
inline std::uintptr_t concurrent_object_pool<Object, Allocator>::get_block_key_from(const block_node& from)
{
	if (from.m_blockKey)
		return from.m_blockKey;

	const gdul::shared_ptr<Object[]> objects(from.m_objects.load(std::memory_order_relaxed));

	return to_block_key(objects.get(), (std::uint32_t)objects.item_count());
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::discard_object(const Object* obj)
{
	const std::uint8_t blockCount(m_blocksEndIndex.load(std::memory_order_acquire));

	for (std::uint8_t i = 0; i < blockCount; ++i) {
		if (is_contained_within(obj, m_blocks[i].m_blockKey)) {
			if (m_blocks[i].m_livingObjects.fetch_sub(1, std::memory_order_relaxed) == 1) {
				m_blocks[i].m_objects.store(gdul::shared_ptr<Object[]>(nullptr));
			}
			return;
		}
	}
	assert(false && "Object does not belong in this structure");
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::force_hint_update()
{
	// Keep writing to hint until m_blockEndIndex is stable
	std::uint8_t preHintUpdate(0);
	std::uint8_t postHintUpdate(m_blocksEndIndex.load(std::memory_order_relaxed));
	do {
		preHintUpdate = postHintUpdate;
		m_activeBlockKeyHint.store(m_blocks[preHintUpdate - 1].m_blockKey, std::memory_order_relaxed);
		postHintUpdate = m_blocksEndIndex.load(std::memory_order_relaxed);
	} while (postHintUpdate != preHintUpdate);
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::try_alloc_block(std::uint8_t blockIndex)
{
	assert((blockIndex < CapacityBits) && "Capacity overflow");

	if (!(blockIndex < CapacityBits))
		return;

	shared_ptr<Object[]> expected(m_blocks[blockIndex].m_objects.load(std::memory_order_relaxed));

	if (expected.get_version() == 2)
		return;

	const std::uint32_t size((std::uint32_t)std::powf(2.f, (float)blockIndex + 1));

	if (expected.get_version() == 0) {

		shared_ptr<Object[]> desired(gdul::allocate_shared<Object[]>(size, m_allocator));
		if (m_blocks[blockIndex].m_objects.compare_exchange_strong(expected, desired, std::memory_order_release)) {
			expected = std::move(desired);
		}
	}

	const std::uintptr_t blockKey(to_block_key(expected.get(), (std::uint32_t)expected.item_count()));
	m_blocks[blockIndex].m_blockKey = blockKey;

	std::uint32_t pushIndex(m_blocks[blockIndex].m_pushSync.fetch_add(1, std::memory_order_relaxed));
	while (pushIndex < size) {
		m_unusedObjects.push(&expected[pushIndex]);
		pushIndex = m_blocks[blockIndex].m_pushSync.fetch_add(1, std::memory_order_relaxed);
	}

	m_blocksEndIndex.compare_exchange_strong(blockIndex, blockIndex + 1, std::memory_order_release);

	force_hint_update();
}
}




