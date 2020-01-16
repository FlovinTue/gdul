#pragma once

#include <atomic>
#include <stdint.h>

#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/thread_local_member/thread_local_member.h>

namespace gdul
{

// Would be really cool to make a queue with comparable performance to regular concurrent_queue
// but with (much) less complexity and with best-effort FIFO
template <class T, class Allocator>
class concurrent_queue_fifo
{
public:

	using value_type = T;
	using allocator_type = Allocator;
	using size_type = std::size_t;
	using item_type = struct { T m_item; };

	template <class U>
	void push(U&& in);
	bool try_pop(T& out);

	using iterator = item_type * ;
	using const_iterator = const item_type*;
	using reverse_iterator = iterator;
	using const_reverse_iterator = const_iterator;

private:
	struct items_node
	{
		atomic_shared_ptr<items_node> m_next;
		shared_ptr<item_type[]> m_items;
	};
	
	iterator begin();
	const_iterator cbegin() const;
	iterator end();
	const_iterator cend() const;

	void push_items_node(shared_ptr<items_node> node);
	void try_pop_items_node();

	tlm<shared_ptr<item_type[]>> t_items;
	
	// Keep a single hierarchy of items_nodes.
	atomic_shared_ptr<items_node> m_items;
	
	std::atomic<iterator> m_writeAt;
	std::atomic<iterator> m_writeRear;
	std::atomic<iterator> m_readAt;
	std::atomic<iterator> m_readRear;
};

template<class T, class Allocator>
template<class U>
inline void concurrent_queue_fifo<T, Allocator>::push(U && in)
{
	// iterator itr = acquire iterator;
	// while (itr outside)
	// push new array
	// itr = acquire iterator

	// write(std::forward<U>(in), itr)
}
template<class T, class Allocator>
inline bool concurrent_queue_fifo<T, Allocator>::try_pop(T & out)
{
	// iterator itr = acquire iterator;
	// while (itr outside)
	// if (!swap_items_array())
	// break

	// itr = acquire iterator
	
	// write(*itr, (iterator)&out)
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::iterator concurrent_queue_fifo<T, Allocator>::begin()
{
	return t_items.get().get();
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::const_iterator concurrent_queue_fifo<T, Allocator>::cbegin() const
{
	return t_items.get().get();
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::iterator concurrent_queue_fifo<T, Allocator>::end()
{
	return t_items.get().get();
}

template<class T, class Allocator>
inline typename concurrent_queue_fifo<T, Allocator>::const_iterator concurrent_queue_fifo<T, Allocator>::cend() const
{
	return t_items.get().get();
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::push_items_node(shared_ptr<items_node> node)
{
	// Probably races here...

	atomic_shared_ptr<items_node>* rawref(&m_items);
	do{
		shared_ptr<items_node> ref(rawref.load(std::memory_order_relaxed));

		while (*rawref){
			rawref = &ref->m_next;
			ref->m_next.load(std::memory_order_relaxed);
		}

		raw_ptr<items_node> exp(nullptr, 0);
	} while (!rawref.compare_exchange_strong(exp, std::move(node), std::memory_order_acquire, std::memory_order_relaxed));
}
template<class T, class Allocator>
inline void concurrent_queue_fifo<T, Allocator>::try_pop_items_node()
{
	shared_ptr<items_node> items(m_items.load(std::memory_order_relaxed);

	if (!items){
		return;
	}

	shared_ptr<items_node> desired(items->m_next.load(std::memory_order_relaxed));

	raw_ptr<items_node> exp(items);
	m_items.compare_exchange_strong(exp, std::move(desired));
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