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
#include <gdul/concurrent_queue/concurrent_queue.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

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
		shared_ptr<Object[]> m_objects;
		shared_ptr<block_node> m_next;
	};

	concurrent_queue<Object*, Allocator> m_unusedObjects;

	atomic_shared_ptr<block_node> m_lastBlock;

	const std::size_t m_blockSize;

	Allocator m_allocator;
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
	shared_ptr<block_node> next(m_lastBlock.unsafe_exchange(nullptr, std::memory_order_relaxed));

	while (next)
	{
		next = std::move(next->m_next);
	}

	m_unusedObjects.unsafe_reset();
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::try_alloc_block()
{
	raw_ptr<block_node> exp(m_lastBlock.get_raw_ptr());

	shared_ptr<block_node> node(make_shared<block_node, Allocator>(m_allocator));
	node->m_next = m_lastBlock.load();
	node->m_objects = make_shared<Object[], Allocator>(m_blockSize, m_allocator);

	if (m_unusedObjects.size()){
		return;
	}

	if (m_lastBlock.compare_exchange_strong(exp, node)){
		for (std::size_t i = 0; i < m_blockSize; ++i){
			m_unusedObjects.push(&node->m_objects[i]);
		}
	}
}
}