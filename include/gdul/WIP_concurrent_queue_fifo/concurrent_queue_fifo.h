#pragma once

#include <array>
#include <atomic>
#include <stdint.h>

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/thread_local_member/thread_local_member.h>

namespace gdul
{

// Would be really cool to make a queue with comparable performance to regular concurrent_queue
// but with (much) less complexity and with best-effort FIFO

namespace cq_fifo_detail
{
using size_type = std::size_t;
static constexpr size_type Max_Capacity = std::numeric_limits<size_type>::max() / 2;
}

template <class T, class Allocator = std::allocator<T>>
class concurrent_queue_fifo
{
public:
	using value_type = T;
	using allocator_type = Allocator;
	using size_type = typename cq_fifo_detail::size_type;
	using item_type = struct { T m_item; bool m_written; };

	concurrent_queue_fifo() = default;
	concurrent_queue_fifo(size_type initialCapacity);

	template <class U>
	void push(U&& in);
	bool try_pop(T& out);

private:
	void try_advance_write_check(size_type from, std::atomic<size_type>& index, std::atomic<size_type>& performed, bool desirableValue);

	shared_ptr<item_type[]> m_items;
	
	std::atomic<size_type> m_writeReserve;
	std::atomic<size_type> m_readReserve;

	std::atomic<size_type> m_writeAt;
	std::atomic<size_type> m_readAt;

	std::atomic<size_type> m_written;
	std::atomic<size_type> m_read;
};
template<class T, class Allocator>
inline concurrent_queue_fifo<T, Allocator>::concurrent_queue_fifo(size_type initialCapacity)
	: m_items(gdul::make_shared<item_type[]>(initialCapacity))
	, m_writeReserve(0)
	, m_readReserve(0)
	, m_writeAt(0)
	, m_readAt(0)
	, m_written(0)
	, m_read(0)
{
}
template<class T, class Allocator>
template<class U>
inline void concurrent_queue_fifo<T, Allocator>::push(U && in)
{
	const size_type reserve(m_writeReserve.fetch_add(1));
	const size_type read(m_read.load());
	const size_type used(reserve - read);
	const size_type capacity(m_items.item_count());
	const size_type avaliable(capacity - used);

	if (!((avaliable - 1) < capacity)){
		m_writeReserve.fetch_sub(1);
		return;
	}

	const size_type at(m_writeAt.fetch_add(1));
	const size_type atLocal(at % capacity);

	m_items[atLocal].m_item = std::forward<U>(in);
	m_items[atLocal].m_written = true;

	try_advance_write_check(at, m_writeAt, m_written, true);
}
template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::try_pop(T & out)
{
	const size_type reserve(m_readReserve.fetch_add(1));
	const size_type written(m_written.load());
	
	if (!(reserve < written)){
		m_readReserve.fetch_sub(1);
		return false;
	}

	const size_type capacity(m_items.item_count());
	const size_type at(m_readAt.fetch_add(1));
	const size_type atLocal(at % capacity);

	out = std::move(m_items[atLocal].m_item);

	m_items[atLocal].m_written = false;

	try_advance_write_check(at, m_readAt, m_read, false);

	return true;
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::try_advance_write_check(size_type from, std::atomic<size_type>& index, std::atomic<size_type>& performed, bool desirableValue)
{
	from;

	size_type perf(performed.load());
	size_type ix;
	while (ix = index.load() != perf) {
		
		size_type incr(0);
		for (size_type i = from; i < ix; ++i, ++incr){
			if (m_items[i % m_items.item_count()].m_written != desirableValue){
				break;
			}
		}

		if (!incr) {
			break;
		}

		if (performed.compare_exchange_strong(perf, perf + incr)) {
			break;
		}
	}
}
}

// Want to keep things simple. No weird block construction one after another thing.
// Maybe keep a separate linked list? 

// Consumers will only change t_items whenever they run out of items.. 
// Which would then look like: 
// If (!t_items.try_pop(out)){
// look forward in list
// if (validQueue)
// try_swap_active_items(newItems);

// Use buffer-list to push overflowing entries. Overflowing being of the definition not looping back to index 0
// This list should at most be two nodes long. After that, grow capacity... 

// Previous idea of having backmost (what defines backmost?) writer scan forward to 'push' a rear writeIndex

// Doubtful, since this was heavily thought of during first iteration. This would, however, allow readers to use
// the concurrent_queue producer_buffer algorithm.

// We want to avoid using a cas loop (with common failure), relying on atomic increments; GO FORWARD! : D

// So. Each writer try to increment rear writeIndex from its writeAt up to m_writeAt? Would need to keep state in each item. (That was 
// always the case with this strategy)

// Say we DO keep state in each item, it would have to be atomic (I think?). Or not. CAS would have to be used for increments though (
// so as not to doubly-increment)...

// Really smells like there are races somewhere inside that idea though. Potentially fundamental, unavoidable...

// It would (might) go: 
// writer finishes writing. sets item state to written.
// 
// then
//
//	if (rear index < writeAt)
//		do nothing
//	else
//		while(get_item_state(forwardScanIndex) == item_state_valid)
//			try_increment_rear_write_index();	
// 

// And what of readers? The first two parts of the concurrent_queue producer_buffer mechanism can be used, but the 
// item state part?
// Possible to inform writers of rear readIndex ?

// Will 'failed' items be accepted? THE HORROR!