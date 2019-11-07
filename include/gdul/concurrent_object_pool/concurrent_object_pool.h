// Copyright(c) 2019 Flovin Michaelsen
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

#include <assert.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <atomic>

#undef get_object

namespace gdul {

template <class Object, class Allocator = std::allocator<std::uint8_t>>
class concurrent_object_pool
{
public:

	concurrent_object_pool(std::size_t blockSize);
	concurrent_object_pool(std::size_t blockSize, Allocator& allocator);
	~concurrent_object_pool();

	inline Object* get_object();
	inline void recycle_object(Object* object);

	inline std::size_t avaliable();

	inline void unsafe_destroy();

private:
	void try_alloc_block();

	struct block_node
	{
		std::uint8_t* m_block;
		Object* m_objects;
		std::size_t m_byteSize;
		std::atomic<block_node*> m_previous;
	};

	using object_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<uint8_t>;
	using node_allocator = typename std::allocator_traits<Allocator>::template rebind_alloc<block_node>;

	Allocator m_allocator;
	node_allocator m_nodeAllocator;

	concurrent_queue<Object*, Allocator> m_unusedObjects;

	std::atomic<block_node*> m_lastBlock;

	const std::size_t m_blockSize;
};

template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize)
	: m_blockSize(blockSize)
	, m_unusedObjects(blockSize, m_allocator)
	, m_lastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize, Allocator & allocator)
	: m_allocator(allocator)
	, m_nodeAllocator(allocator)
	, m_blockSize(blockSize)
	, m_unusedObjects(blockSize, allocator)
	, m_lastBlock(nullptr)
{
	try_alloc_block();
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::~concurrent_object_pool()
{
	unsafe_destroy();
}
template<class Object, class Allocator>
inline Object * concurrent_object_pool<Object, Allocator>::get_object()
{
	Object* out;

	while (!m_unusedObjects.try_pop(out)) {
		try_alloc_block();
	}
	return out;
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::recycle_object(Object * object)
{
	m_unusedObjects.push(object);
}
template<class Object, class Allocator>
inline std::size_t concurrent_object_pool<Object, Allocator>::avaliable()
{
	return m_unusedObjects.size();
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::unsafe_destroy()
{
	block_node* blockNode(m_lastBlock.load(std::memory_order_acquire));
	while (blockNode) {
		block_node* const previous(blockNode->m_previous);

		for (std::size_t i = 0; i < m_blockSize; ++i) {
			blockNode->m_objects[i].~Object();
		}
		m_allocator.deallocate(blockNode->m_block, blockNode->m_byteSize);

		blockNode->~block_node();
		m_nodeAllocator.deallocate(blockNode, 1);

		blockNode = previous;
	}
	m_lastBlock = nullptr;

	m_unusedObjects.unsafe_reset();
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::try_alloc_block()
{
	block_node* expected(m_lastBlock.load(std::memory_order_acquire));

	if (m_unusedObjects.size()) {
		return;
	}

	const std::size_t allocSize(m_blockSize * sizeof(Object));
	const std::size_t align(alignof(Object));
	const std::size_t alignComp(!(8 < align) ? 0 : align);
	const std::size_t totalAllocSize(allocSize + alignComp);

	uint8_t* const block(m_allocator.allocate(totalAllocSize));

	const std::size_t alignMod((uint64_t)block % align);
	uint8_t* const objectBegin(block + (alignMod ? align - alignMod : 0));
	Object* const objects((Object*)objectBegin);

	for (std::size_t i = 0; i < m_blockSize; ++i) {
		new (&objects[i]) (Object);
	}

	block_node* const desired(m_nodeAllocator.allocate(1));
	new (desired)(block_node);
	desired->m_previous = expected;
	desired->m_block = block;
	desired->m_byteSize = totalAllocSize;
	desired->m_objects = objects;

	if (!m_lastBlock.compare_exchange_strong(expected, desired)) {
		for (std::size_t i = 0; i < m_blockSize; ++i) {
			objects[i].~Object();
		}
		m_allocator.deallocate(block, totalAllocSize);

		desired->~block_node();
		m_nodeAllocator.deallocate(desired, 1);

		return;
	}

	for (std::size_t i = 0; i < m_blockSize; ++i) {
		m_unusedObjects.push(&objects[i]);
	}
}
}