#pragma once

#include <memory>
#include <atomic>
#include <gdul/thread_local_member/thread_local_member.h>

#pragma warning(push)
#pragma warning(disable:4201)

namespace gdul {

namespace csp_detail {
using size_type = std::size_t;

static constexpr size_type Tl_Scratch_Size = 8;
}

template <class T, class Allocator = std::allocator<T>>
class concurrent_scratch_pool
{
public:
	using size_type = typename csp_detail::size_type;

	concurrent_scratch_pool();
	concurrent_scratch_pool(Allocator alloc);
	concurrent_scratch_pool(size_type initialPoolSize);
	concurrent_scratch_pool(size_type initialPoolSize, Allocator alloc);
	~concurrent_scratch_pool();

	// Concurrency safe
	T* get();

	// Assumes no outstanding references to objects as well as no concurrent access
	void unsafe_reset();

private:

	struct excess_block
	{
		shared_ptr<T[]> m_items;
		shared_ptr<excess_block> m_next;
	};

	struct alignas(std::hardware_destructive_interference_size)
	{
		std::atomic<size_type> m_indexClaim;
		atomic_shared_ptr<excess_block> m_excessBlocks;
	};

	struct tl_container
	{
		size_type iteration;
		size_type localScratchIndex;
		T* localScratch;
	};
	void destroy_excess();

	void reset_tl(tl_container& tl);
	void reset_tl_scratch(tl_container& tl);

	T* acquire_tl_scratch(size_type atIndex);

	tlm<tl_container> t_details;
	shared_ptr<T[]> m_block;
	std::size_t m_iteration;

	Allocator m_allocator;
};



template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool()
	: concurrent_scratch_pool(csp_detail::Tl_Scratch_Size, Allocator())
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(Allocator alloc)
	: concurrent_scratch_pool(csp_detail::Tl_Scratch_Size, alloc)
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(size_type initialPoolSize)
	: concurrent_scratch_pool(initialPoolSize, Allocator())
{
}

template<class T, class Allocator>
inline concurrent_scratch_pool<T, Allocator>::concurrent_scratch_pool(size_type initialPoolSize, Allocator alloc)
	: t_details(tl_container{ 0, 0, nullptr }, alloc)
	, m_block(allocate_shared<T[]>(((initialPoolSize / csp_detail::Tl_Scratch_Size + ((bool)(initialPoolSize % csp_detail::Tl_Scratch_Size)))* csp_detail::Tl_Scratch_Size /* align value to Tl_Scratch_Size */), alloc))
	, m_iteration(1)
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
		reset_tl(tl);

		return &tl.localScratch[tl.localScratchIndex++];
	}

	if (!(tl.localScratchIndex < csp_detail::Tl_Scratch_Size)) {
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
		m_block = allocate_shared<T[]>(claimed);
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
inline void concurrent_scratch_pool<T, Allocator>::reset_tl(tl_container& tl)
{
	tl.iteration = m_iteration;
	reset_tl_scratch(tl);
}

template<class T, class Allocator>
inline void concurrent_scratch_pool<T, Allocator>::reset_tl_scratch(tl_container& tl)
{
	tl.localScratchIndex = 0;
	tl.localScratch = acquire_tl_scratch(m_indexClaim.fetch_add(csp_detail::Tl_Scratch_Size));
}

template<class T, class Allocator>
inline T* concurrent_scratch_pool<T, Allocator>::acquire_tl_scratch(size_type atIndex)
{
	if (atIndex < m_block.item_count()) {
		return &m_block[atIndex];
	}

	shared_ptr<excess_block> node(gdul::allocate_shared<excess_block>(m_allocator));
	node->m_items = gdul::allocate_shared<T[]>(csp_detail::Tl_Scratch_Size, m_allocator);

	T* const ret(node->m_items.get());

	shared_ptr<excess_block> expected(m_excessBlocks.load());

	do {
		node->m_next = expected;
	} while (!m_excessBlocks.compare_exchange_strong(expected, std::move(node)));

	return ret;
}
}
#pragma warning(pop)