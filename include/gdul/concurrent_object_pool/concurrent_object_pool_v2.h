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

#include <assert.h>
#include <gdul/thread_local_member/thread_local_member.h>
#include <queue>
#include <thread>

#undef get_object

namespace gdul {

template <class Object, class Allocator = std::allocator<std::uint8_t>>
class concurrent_object_pool
{
public:
	concurrent_object_pool();
	concurrent_object_pool(Allocator allocator);
	concurrent_object_pool(std::size_t blockSize);
	concurrent_object_pool(std::size_t blockSize, Allocator allocator);
	~concurrent_object_pool();

	// Threads will cache maximally these many from the shared objects at any one time
	static constexpr std::size_t Mini_Block_Max_Size = 16;
	static constexpr std::size_t Default_Block_Size = 16;

	void try_alloc_block();

	inline Object* get();
	inline void recycle(Object* object);

	inline std::size_t avaliable() const;
	inline std::size_t block_size() const;

	inline void unsafe_reset();
	inline void unsafe_reset(std::size_t blockSize);

private:
	bool try_claim_mini_block();

	tlm<std::queue<Object*>> t_objectCache;

	struct block_node
	{
		shared_ptr<Object[]> m_objects;
		shared_ptr<block_node> m_next;
		std::atomic<std::size_t> m_claimIndex;
	};

	atomic_shared_ptr<block_node> m_currentBlock;

	std::size_t m_blockSize;

	Allocator m_allocator;
};
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool()
	: concurrent_object_pool(Default_Block_Size, Allocator())
{}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(Allocator allocator)
	: concurrent_object_pool(Default_Block_Size, allocator)
{}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize)
	: concurrent_object_pool(blockSize, Allocator())
{
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::concurrent_object_pool(std::size_t blockSize, Allocator allocator)
	: m_allocator(allocator)
	, m_blockSize(blockSize)
{
}
template<class Object, class Allocator>
inline concurrent_object_pool<Object, Allocator>::~concurrent_object_pool()
{
	unsafe_reset();
}
template<class Object, class Allocator>
inline Object* concurrent_object_pool<Object, Allocator>::get()
{
	if (t_objectCache.get().empty()) {
		while (!try_claim_mini_block())
			try_alloc_block();
	}

	Object* const ret(t_objectCache.get().front());

	t_objectCache.get().pop();

	return ret;
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::recycle(Object* object)
{
	t_objectCache.get().push(object);
}
template<class Object, class Allocator>
inline bool concurrent_object_pool<Object, Allocator>::try_claim_mini_block()
{
	const std::size_t aSrc(m_blockSize / std::thread::hardware_concurrency());
	const std::size_t bSrc(Mini_Block_Max_Size);

	const std::size_t desiredClaim(aSrc < bSrc ? aSrc : bSrc);
	const std::size_t minClaim(1);
	const std::size_t attemptClaim(minClaim < desiredClaim ? desiredClaim : minClaim);

	shared_ptr<block_node> topBlock(m_currentBlock.load(std::memory_order_relaxed));

	if (!topBlock) {
		return false;
	}

	const std::size_t result(topBlock->m_claimIndex.fetch_add(attemptClaim, std::memory_order_relaxed));
	if (!(result < m_blockSize)) {
		return false;
	}
	const std::size_t maxClaim(m_blockSize - result);
	const std::size_t actualClaim(maxClaim < attemptClaim ? maxClaim : attemptClaim);

	raw_ptr<Object[]> objects(topBlock->m_objects);
	std::queue<Object*>& cache(t_objectCache.get());

	for (std::size_t i = result; i < result + actualClaim; ++i) {
		cache.push(&objects[i]);
	}

	return true;
}
template<class Object, class Allocator>
inline std::size_t concurrent_object_pool<Object, Allocator>::avaliable() const
{
	return t_objectCache.get().size();
}
template<class Object, class Allocator>
inline std::size_t concurrent_object_pool<Object, Allocator>::block_size() const
{
	return m_blockSize;
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::unsafe_reset()
{
	unsafe_reset(m_blockSize);
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::unsafe_reset(std::size_t blockSize)
{
	shared_ptr<block_node> next(m_currentBlock.unsafe_exchange(shared_ptr<block_node>(nullptr), std::memory_order_relaxed));

	while (next)
		next = std::move(next->m_next);
}
template<class Object, class Allocator>
inline void concurrent_object_pool<Object, Allocator>::try_alloc_block()
{
	shared_ptr<block_node> exp(m_currentBlock.load(std::memory_order_relaxed));

	if (exp && exp->m_claimIndex.load() < m_blockSize)
		return;

	shared_ptr<block_node> node(gdul::allocate_shared<block_node>(m_allocator));
	node->m_next = m_currentBlock.load();
	node->m_objects = gdul::allocate_shared<Object[]>(m_blockSize, m_allocator);
	node->m_claimIndex.store(0, std::memory_order_relaxed);

	m_currentBlock.compare_exchange_strong(exp, node, std::memory_order_relaxed);
}
}