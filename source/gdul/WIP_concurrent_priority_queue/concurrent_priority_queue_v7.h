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
#include <utility>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

// De-linking happens at the previous node. So if there are deleters declaring deletion at next node,
// .......
//
//
//			N1 -- > N2 ---> N3
//
//			Linking a new node between N2 & N3 will require altering m_next at N2
//			If deletion is flagged before loading [m_next -> N3] the node may be skipped altogether.
//			If deletion is flagged after loading [m_next-> N3] the link will fail
//

// The good thing about not generalizing the functionality is that a deleter may assume that it wants to delete 'next' without
// any inspection. This means, going from beginning, we may declare our intent beforehand.

// So in the case above. Flagging N3 for deletion would mean we set state in N2 m_next to erase_next

// So. This would in essence be ref-counting. If anyone encounters a node with the deletion state it must attempt a deletion help? Yes. What if we load
// N3 -> m_next and inc refs there? .. - Before deleting N2, or worse, delete N2 
// Convolution. Should attempt to enforce simplicity. (this might be solved by atomic_shared_ptr)

// An inserter enters N2, loads m_next & increfs, stalls
// A deleter enters N2, loads m_next, flags & increfs. Now what? I suppose a special case could be made ... No.. Hmm well it's only memory reclamation.
// The node only needs to outlive the inserter.

// Just a second. So the inserters will never need to consider that other inserters might remove a node. Other inserters will only ever obstruct insertion by more insertion.
// And deleters have a very strict way of operating. They may only delete serially from the front (although somewhat parallelized).

// Would be pretty sweet using atomic_shared_ptr. That would solve all ref counting troubles.

// How 'bout using node refs for own node? (Regular way)
namespace gdul {
namespace cpq_detail {

static constexpr std::uintptr_t Bottom_Bits = 1 << 1 << 1;
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Bottom_Bits;
static constexpr std::uintptr_t Erase_Ref_Step = std::uintptr_t(1) << 48;

using size_type = std::size_t;

struct node_state {
	enum : std::uintptr_t{
		erase_next = 1 << 0,
		erase_current = 1 << 1,
	};
};

static bool valid_index(std::uint16_t index, std::uint16_t lastRefs, const std::atomic<std::uintptr_t>& head);

template <class Key, class Value>
struct alignas(alignof(std::max_align_t)) node
{
	node() : m_kv(), m_next(nullptr) {}
	node(std::pair<Key, Value>&& item) : node(std::move(item), nullptr){}
	node(const std::pair<Key, Value>& item) : node(item, nullptr){}
	node(std::pair<Key, Value>&& item, node* next) : m_kv(std::move(item)), m_next(next) {}
	node(const std::pair<Key, Value>& item, node* next) : m_kv(item), m_next(next) {}

	struct node_view {
		node_view() = default;
		node_view(std::uintptr_t value) :m_value(value) {}
		node_view(node* n) : m_value((std::uintptr_t)(n)) {}

		node* to_node() const {
			return (node*)(m_value & Pointer_Mask);
		}
		union {
			const node* m_nodeView;
			
			std::uintptr_t m_value;
			struct {
				std::uint16_t padding[3];
				std::uint16_t m_refs;
			};
		};
	};
	union node_rep
	{
		node_rep(): m_value(0) {}
		node_rep(std::uintptr_t value) : m_value(value){}
		node_rep(const node_view& view) : m_value(view.m_value){}

		inline std::uintptr_t inc_erase_ref() {
			return m_value.fetch_add(Erase_Ref_Step) + Erase_Ref_Step;
		}
		inline node_view load() const {
			return m_value.load();
		}

		const node* const m_nodeView;
		std::atomic<std::uintptr_t> m_value;
	};


	std::pair<Key, Value> m_kv;

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
	bool try_pop(node_type*& out);

private:
	using node_rep = typename cpq_detail::node<Key, Value>::node_rep;
	using node_view = typename cpq_detail::node<Key, Value>::node_view;

	void try_exchange_head(std::uintptr_t expected, node_type* desired);

	static bool try_link(node_type* node, std::uintptr_t at, std::uintptr_t expectingNext);
	static std::uintptr_t step_forward(std::uintptr_t from, std::uint16_t steps);


	allocator_type m_allocator;
	compare_type m_comparator;

	// Also ->end
	node_type m_head;

	const std::uint8_t _padding[std::hardware_destructive_interference_size]{};

	// ToDo: This should probably be moved in to node_rep instead, using 8bits for recentRefs &
	// 8bits for refs. Might need to use compare_exchange instead of fetch_add then.. 
	std::atomic<std::uint16_t> m_recentRefs;
};

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue() 
	: m_head{ std::make_pair(std::numeric_limits<Key>::max(), Value()) , &m_head}
	, m_recentRefs(0)
{}
template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::~concurrent_priority_queue()
{}
template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	// The idea of synchronizing with deleters is that *all* nodes from m_head -> (m_head.m_refs - m_recentRefs) will be claimed for
	// deletion. If an inserter is able to synchronize with this value securely, and abort in case of delete interference,
	// that would mean inserters only have other inserters to contend with..... In which case there is no danger (concurrent insertion
	// is pretty simple, on it's own).

	// The way this could work would be to load recentRefs - > track steps forward (beyond current delete-claimed)-> link in ->
	// reload head to make sure link was not attached to deleted node.


	do {
		const std::uint16_t recentRefs(m_recentRefs.load());

		node_view current(&m_head);
		
		std::uint16_t stepped(current.m_refs - recentRefs);
		node_view next(step_forward(current.to_node()->m_next.m_value.load(), stepped));

		for (;;) {
			if (!m_comparator(next.to_node()->m_kv.first, node->m_kv.first)) {
				// Next fails comparator test, try link after current

				if (try_link(node, current.m_value, next.m_value)) {
					// Link succeeded, now recheck to make sure we're not in deletion territory.

					if (cpq_detail::valid_index(stepped, recentRefs, m_head.m_next.m_value)){
						// We've linked at a valid index, all done!
						  return;
					}
				}
				else {
					// This'll be another inserter linking after current. Need to reload next
					next = current.to_node()->m_next.load();
				}
			}
			else {
				current = next;
				next = step_forward(current.m_value, 1);
				++stepped;
			}
		}

	} while (true);

}


template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
	// So, the deleter will attempt to splice m_next onto head rather than the previous node.
	// If several deleters operate concurrently only the last one will succeed. 

	node_view head(m_head.m_next.inc_erase_ref());

	node_type* current(&m_head);
	node_type* next((node_type*)(head.m_value & cpq_detail::Pointer_Mask));
	
	for (size_type i = 0; i < head.m_refs; ++i) {
		if (next == &m_head) {
			try_exchange_head(head.m_value, &m_head);
			return false;
		}

		current = next;

		
		next = (node_type*)(next->m_next.inc_erase_ref() & cpq_detail::Pointer_Mask);
	}

#if defined(_DEBUG)
	current->m_next.m_value.fetch_and(cpq_detail::Pointer_Mask);
#endif

	try_exchange_head(head.m_value, next);

	out = current;

	return true;
}
template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::try_exchange_head(std::uintptr_t expected, node_type* desired)
{
	m_head.m_next.m_value.compare_exchange_strong(expected, (std::uintptr_t)(desired));
}
template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link(node_type* node, std::uintptr_t at, std::uintptr_t expectingNext) {
	node_view current(at);

	node->m_next.m_value.store(expectingNext);
	return current.to_node()->m_next.m_value.compare_exchange_weak(expectingNext, (std::uintptr_t)node);
}

template<class Key, class Value, class Compare, class Allocator>
inline std::uintptr_t concurrent_priority_queue<Key, Value, Compare, Allocator>::step_forward(std::uintptr_t from, std::uint16_t steps) {
	node_view current(from);

	for (std::uint16_t i = 0; i < steps; ++i) {
		if (!current.to_node()) {
			break;
		}
		current = current.to_node()->m_next.m_value.load();
	}
	return current.m_value;
}


namespace cpq_detail {
static bool valid_index(std::uint16_t index, std::uint16_t lastRefs, const std::atomic<std::uintptr_t>& head) {
	union
	{
		std::uintptr_t value;
		struct
		{
			std::uint16_t padding[3];
			std::uint16_t refs;
		};
	};
	value = head.load();

	const std::uint16_t deltaRefs(refs - lastRefs);

	return !(index < deltaRefs);
}
}
}

#pragma warning(pop)






















// ----------- OLD--------------------------------------------
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

	/*std::uintptr_t headValue(m_head.m_value.fetch_add(1));
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

	return true;*/

