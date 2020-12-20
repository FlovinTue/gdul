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

#include <memory>
#include <atomic>
#include <cmath>
#include <gdul/thread_local_member/thread_local_member.h>

#pragma warning(push)
// Anonymous struct
#pragma warning(disable:4201)

namespace gdul {

namespace csp_detail {
using size_type = std::size_t;

inline size_type pow2_align(size_type from, size_type clamp = std::numeric_limits<size_type>::max());

static constexpr size_type Default_Scratch_Size = 128;
static constexpr size_type Default_Tl_Cache_Size = 32;
}

/// <summary>
/// Concurrency safe lock-free scratch pool for efficient object reuse
/// </summary>
template <class T, class Allocator = std::allocator<std::uint8_t>>
class concurrent_scratch_pool
{
public:
	using size_type = typename csp_detail::size_type;
	using allocator_type = Allocator;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_scratch_pool();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="alloc">Allocator to use for blocks</param>
	concurrent_scratch_pool(Allocator alloc);

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="InitialScratchSize">Initial size of the scratch block</param>
	/// <param name="tlCacheSize">Size of the thread local caches</param>
	concurrent_scratch_pool(size_type InitialScratchSize, size_type tlCacheSize);

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="InitialScratchSize">Initial size of the scratch block</param>
	/// <param name="tlCacheSize">Size of the thread local caches</param>
	/// <param name="alloc">Allocator to use for blocks</param>
	concurrent_scratch_pool(size_type InitialScratchSize, size_type tlCacheSize, Allocator alloc);

	/// <summary>
	/// Destructor
	/// </summary>
	~concurrent_scratch_pool();

	/// <summary>
	/// Fetch item
	/// </summary>
	/// <returns>Unused item</returns>
	T* get();

	/// <summary>
	/// Resets the scratch block for reuse. Assumes no outstanding item references and 
	/// exclusive access
	/// </summary>
	void unsafe_reset();

private:
	struct excess_block
	{
		shared_ptr<T[]> m_items;
		shared_ptr<excess_block> m_next;
	};

	struct tl_container
	{
		size_type iteration;
		size_type localScratchIndex;
		T* localScratch;
	};
	void destroy_excess();

	void reset_tl_container(tl_container& tl);
	void reset_tl_scratch(tl_container& tl);

	T* acquire_tl_scratch();

	struct /* anonymous struct */ alignas(std::hardware_destructive_interference_size) 
	{
		atomic_shared_ptr<excess_block> m_excessBlocks;
		std::atomic<size_type> m_indexClaim;
	};

	tlm<tl_container> t_details;
	shared_ptr<T[]> m_block;
	size_type m_iteration;
	size_type m_tlCacheSize;
	Allocator m_allocator;
};



template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool()
	: concurrent_scratch_pool(csp_detail::Default_Scratch_Size, csp_detail::Default_Tl_Cache_Size, Allocator())
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(Allocator alloc)
	: concurrent_scratch_pool(csp_detail::Default_Scratch_Size, csp_detail::Default_Tl_Cache_Size, alloc)
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(size_type InitialScratchSize, size_type tlCacheSize)
	: concurrent_scratch_pool(InitialScratchSize, Allocator())
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(size_type InitialScratchSize, size_type tlCacheSize, Allocator alloc)
	: t_details(tl_container{ 0, 0, nullptr }, alloc)
	, m_block(allocate_shared<T[]>(csp_detail::pow2_align(InitialScratchSize), alloc))
	, m_iteration(1)
	, m_tlCacheSize(csp_detail::pow2_align(tlCacheSize, csp_detail::pow2_align(InitialScratchSize)))
	, m_allocator(alloc)
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::~concurrent_scratch_pool()
{
	destroy_excess();
}

template<class T, class Allocator>
inline T* concurrent_scratch_pool<T, Allocator>::get()
{
	tl_container& tl(t_details);

	if (tl.iteration < m_iteration) {
		reset_tl_container(tl);

		return &tl.localScratch[tl.localScratchIndex++];
	}

	if (!(tl.localScratchIndex < csp_detail::Default_Tl_Cache_Size)) {
		reset_tl_scratch(tl);
	}

	return &tl.localScratch[tl.localScratchIndex++];
}

template<class T, class Allocator>
inline void concurrent_scratch_pool<T, Allocator>::unsafe_reset()
{
	destroy_excess();

	const size_type claimed(m_indexClaim.exchange(0, std::memory_order_relaxed));

	if (m_block.item_count() < claimed) {
		m_block = gdul::allocate_shared<T[]>(claimed, m_allocator);
	}

	++m_iteration;
}

template<class T, class Allocator>
inline void concurrent_scratch_pool<T, Allocator>::destroy_excess()
{
	shared_ptr<excess_block> excess(m_excessBlocks.unsafe_exchange(shared_ptr<excess_block>()));
	while (excess) {
		excess = std::move(excess->m_next);
	}
}

template<class T, class Allocator>
inline void concurrent_scratch_pool<T, Allocator>::reset_tl_container(tl_container& tl)
{
	tl.iteration = m_iteration;
	reset_tl_scratch(tl);
}

template<class T, class Allocator>
inline void concurrent_scratch_pool<T, Allocator>::reset_tl_scratch(tl_container& tl)
{
	tl.localScratchIndex = 0;
	tl.localScratch = acquire_tl_scratch();
}

template<class T, class Allocator>
inline T* concurrent_scratch_pool<T, Allocator>::acquire_tl_scratch()
{
	const size_type index(m_indexClaim.fetch_add(csp_detail::Default_Tl_Cache_Size, std::memory_order_relaxed));

	if (index < m_block.item_count()) {
		return &m_block[index];
	}

	shared_ptr<excess_block> node(gdul::allocate_shared<excess_block>(m_allocator));
	node->m_items = gdul::allocate_shared<T[]>(csp_detail::Default_Tl_Cache_Size, m_allocator);

	T* const ret(node->m_items.get());

	shared_ptr<excess_block> expected(m_excessBlocks.load(std::memory_order_relaxed));

	do {
		node->m_next = expected;
	} while (!m_excessBlocks.compare_exchange_strong(expected, std::move(node), std::memory_order_relaxed));

	return ret;
}

namespace csp_detail {
inline size_type pow2_align(size_type from, size_type clamp)
{
	const size_type from_(from < 2 ? 2 : from);

	const float flog2(std::log2f(static_cast<float>(from_)));
	const float nextLog2(std::ceil(flog2));
	const float fNextVal(powf(2.f, nextLog2));

	const size_type nextVal(static_cast<size_t>(fNextVal));
	const size_type clampedNextVal((clamp < nextVal) ? clamp : nextVal);

	return clampedNextVal;
}
}
}
#pragma warning(pop)