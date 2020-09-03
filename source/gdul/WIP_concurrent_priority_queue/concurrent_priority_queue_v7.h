#pragma once

// Perhaps it is futile attempting to parallelize this structure. There will always be contention at the front, and deletions can only happen serially.
// Something to think of is that if an entirely sorted set is kept, deletion is super-easy. Simply increment the begin ref.
// If a list is kept, that could be a beginref + offsetCounter, so that the lastmost deletion could win out safely...
// Good thing about a list version would be that it *can* be kept fully sorted..
#include <atomic>
#include <stdint.h>
#include <memory>
#include <thread>
#include <cassert>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)


namespace gdul {
namespace cpq_detail {

static constexpr std::uintptr_t Base_Bits_Mask = std::uintptr_t(1 << 1 << 1 << 1);
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Base_Bits_Mask;

using size_type = std::size_t;


template <class Key, class Value>
struct node
{
	union node_rep
	{
		const node* const m_nodeView;
		std::atomic<std::uintptr_t> m_value;
	};

	std::pair<Key, Value> m_kvPair;

	node_rep m_next;
};
}

template <class Key, class Value, class Compare = std::less<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_priority_queue
{
public:
	using key_type = Key;
	using value_type = Value;
	using compare_type = Compare;
	using size_type = typename cpq_detail::size_type;
	using allocator_type = Allocator;
	using node_type = cpq_detail::node<Key, Value>;

	concurrent_priority_queue();
	~concurrent_priority_queue() noexcept;

	void push(node_type* item);
	bool try_pop(node_type* out);

private:
	using node_rep = typename cpq_detail::node<Key, Value>::node_rep;

	allocator_type m_allocator;
	compare_type m_comparator;

	node_rep m_head;
};

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* item)
{

}


template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* out)
{
	// This would work if?? 
	// * There were no inserts to consider
	// * The nodes were guaranteed to outlive all accesses

	// IF these conditions were filled, the only point of mutation would be in head,
	// and that would be 'progressive'. That is it would only deal with nodes that was previously located
	// further ahead.

	// The second condition can be satisfied by using a frame-sync, ie epoch-based garbage collection.

	// The first condition... ? I mean if an inserter were to load head when a deletion has been declared it could simply 
	// skip by all claimed nodes. However it's fully possible that an inserter loads head -> stalls -> deletion is declared.
	// An inserter could recheck head for declarations, but stalls are always possible. 
	// If a flag is used, a deleter has to declare skip, followed by tagging the node. This will mean
	// an inserter has an opportunity of helping. If 

	// #4-> x1 -> x2 -> x3 -> x4 
	// In this case, as an inserter you may attempt to insert at x1, if successful deleter at x2 will fail.

	std::uintptr_t headValue(m_head.m_value.fetch_add(1));
	const std::uintptr_t skip(headValue & Base_Bits_Mask);

	node_type* head(headValue & cpq_detail::Pointer_Mask);

	if (!head) {
		return false;
	}

	std::uintptr_t replacement(head->m_next.m_value.load());

	for (std::size_t i = 0; i < skip; ++i) {
		head = (node_type*)replacement;
		replacement = head->m_next.m_value.load();
	}

	m_head.m_value.compare_exchange_strong(headValue, replacement);

	out = head;

	return true;
}
}

#pragma warning(pop)