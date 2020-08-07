#pragma once



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

using size_type = std::size_t;

static constexpr std::uint64_t Tag_Mask = 1;
static constexpr std::uint64_t Version_Mask = ~(std::numeric_limits<std::uint64_t>::max() >> 16);
static constexpr std::uint64_t Pointer_Mask = ~Version_Mask & ~Tag_Mask;
static constexpr std::uint64_t Size_Mask = ~Version_Mask;
static constexpr size_type Default_Capacity = 2;

union item_pack {
	item_pack() = default;
	item_pack(std::uint64_t payload)
		: m_payload(payload)
	{}
	std::uint64_t m_payload;
	struct {
		std::uint16_t _padding[3];
		std::uint16_t m_version;
	};
};

template <class Key, class Value>
struct pop_operation;

template <class NodeType>
class alignas(alignof(std::max_align_t)) compound_array;

template <class NodeType>
class alignas(alignof(std::max_align_t)) compound_array
{
public:
	compound_array(size_type capacity)
		: m_next(nullptr)
		, m_capacity(capacity)
		, m_begin((NodeType*)(this + 1))
	{}

	bool contiguous() const {
		return !m_next.load(std::memory_order_relaxed);
	}

	compound_array<NodeType>* next() {
		return m_next.load(std::memory_order_relaxed);
	}
	compound_array<NodeType>* last() {
		compound_array<NodeType>* result(this);
		compound_array<NodeType>* n(next());

		while (n) {
			result = n;
			n = result->next();
		}
		return result;
	}

	bool try_extend(compound_array<NodeType> * with) {
		compound_array<NodeType>* exp(nullptr);
		return m_next.compare_exchange_strong(exp, with);
	}

	NodeType* begin() {
		return (NodeType*)(this + 1);
	}
	const NodeType* begin() const {
		return (NodeType*)(this + 1);
	}

	NodeType& operator[](size_type index) {
		if (index < m_capacity)
			return begin()[index];
		return m_next.load(std::memory_order_relaxed)->begin()[index - m_capacity];
	}
	const NodeType& operator[](size_type index) const {
		if (index < m_capacity)
			return begin()[index];

		return m_next.load(std::memory_order_relaxed)->begin()[index - m_capacity];
	}

	size_type capacity() const {
		const compound_array<NodeType>* const n(m_next.load(std::memory_order_relaxed));
		return n ? local_capacity() + n->capacity() : local_capacity();
	}
	size_type local_capacity() const {
		return m_capacity;
	}

private:
	std::atomic<compound_array<NodeType>*> m_next;

	NodeType* const m_begin;
	size_type const m_capacity;
};

using node_type = std::atomic<std::uint64_t>;
using array_type = compound_array<node_type>;

static item_pack supplant_top(item_pack with, array_type& inArray, std::atomic<size_type>& size);
static void bubble(size_type index, array_type& inArray);
}

template <class Key, class Value, class Compare = std::less<Key>, class Allocator = std::allocator<std::uint8_t>>
class alignas(std::hardware_destructive_interference_size) concurrent_priority_queue
{
public:
	using key_type = Key;
	using value_type = Value;
	using compare_type = Compare;
	using size_type = typename cpq_detail::size_type;
	using allocator_type = Allocator;

	concurrent_priority_queue();
	concurrent_priority_queue(allocator_type allocator);
	concurrent_priority_queue(size_type capacity);
	concurrent_priority_queue(size_type capacity, allocator_type allocator);

	~concurrent_priority_queue() noexcept;

	void push(const std::pair<Key, Value> & item);
	void push(std::pair<Key, Value> && item);

	bool try_pop(std::pair<Key, Value> & out);
	bool try_pop(Value & out);

	void reserve(size_type capacity);

	size_type capacity() const;
	size_type size() const;

	void unsafe_reserve(size_type capacity);
	void unsafe_consolidate();

private:
	template <class NodeType>
	using compound_array = cpq_detail::compound_array<NodeType>;

	using node_type = typename cpq_detail::node_type;
	using array_type = typename cpq_detail::array_type;

	array_type* make_array(size_type capacity, allocator_type allocator) const;
	void destroy_array(array_type * arr, allocator_type allocator) const;
	void copy_to(array_type & to, const array_type & from) const;

	std::atomic<size_type> m_size;

	const std::uint8_t _padding[std::hardware_destructive_interference_size - sizeof(std::atomic<size_type>)];

	std::atomic<array_type*> m_items;

	allocator_type m_allocator;
	compare_type m_comparator;
};

template <class Key, class Value, class Compare, class Allocator>
concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue()
	: concurrent_priority_queue(cpq_detail::Default_Capacity, allocator_type())
{
}

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue(allocator_type allocator)
	: concurrent_priority_queue(cpq_detail::Default_Capacity, allocator)
{
}

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue(size_type capacity)
	: concurrent_priority_queue(capacity, allocator_type())
{
}

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue(size_type capacity, allocator_type allocator)
	: m_size(0)
	, _padding{}
	, m_items(capacity ? make_array(capacity, allocator) : make_array(cpq_detail::Default_Capacity, allocator))
	, m_allocator(allocator)
	, m_comparator(compare_type())
{
}

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::~concurrent_priority_queue() noexcept
{
	destroy_array(m_items.load(std::memory_order_relaxed), m_allocator);
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push(const std::pair<Key, Value>& item)
{
	push(std::make_pair(item.first, item.second));
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push(std::pair<Key, Value>&& item)
{
	// This all feels a bit convoluted.
	// 'tis because of the dual sync needed

	// Perhaps invent a good-enough system where the last layer is not precisely known. Rather, rely on 
	// Items written as the absolute info. .. ? 

	auto& items(*m_items.load());

	std::pair<Key, Value>* const toInsert(new std::pair<Key, Value>(std::move(item)));

	cpq_detail::item_pack sz;
	sz.m_payload = m_size.load();

	for (;;) {
		const size_type end(sz.m_payload & cpq_detail::Size_Mask);
		const size_type cap(items.capacity());

		if (cap < end) {
			reserve(end * 2);
		}

		cpq_detail::item_pack expectedItem;
		expectedItem.m_payload = items[end].load();

		if (expectedItem.m_version != sz.m_version + 1) {
			cpq_detail::item_pack desiredItem;
			desiredItem.m_payload = (std::uint64_t)toInsert;
			desiredItem.m_version = sz.m_version + 1;

			if (items[end].compare_exchange_strong(expectedItem.m_payload, desiredItem.m_payload)) {
				expectedItem = desiredItem;
			}
		}

		// Hmm. Here we allow others to occupy expectedItem, yet try to help
		// If this is not OUR item, we should retry.
		if (expectedItem.m_version == sz.m_version + 1) {
			cpq_detail::item_pack desiredSz;
			desiredSz.m_payload = end + 1;
			desiredSz.m_version = sz.m_version + 1;

			// Still not quite. This will help good, but we have no way of knowing if we were helped
			// or not.

			const bool result(m_size.compare_exchange_strong(sz.m_payload, desiredSz.m_payload));

			if (result) {
				cpq_detail::bubble(end, items);

				if ((expectedItem.m_payload & cpq_detail::Pointer_Mask) == (std::uint64_t)toInsert) {
					break;
				}
			}

		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(std::pair<Key, Value>& out)
{
	auto& items(*m_items.load());

	cpq_detail::item_pack sz;
	sz.m_payload = m_size.load();

	// May use fetch_sub here possibly
	for (std::size_t end(sz.m_payload& cpq_detail::Size_Mask); end; end = sz.m_payload & cpq_detail::Size_Mask) {

		cpq_detail::item_pack back;
		back = items[end - 1].load();

		cpq_detail::item_pack desiredSize;
		desiredSize.m_payload = end - 1;
		desiredSize.m_version = sz.m_version + 1;

		if (m_size.compare_exchange_strong(sz.m_payload, desiredSize.m_payload)) {
			const cpq_detail::item_pack top = cpq_detail::supplant_top(back, items, m_size);

			std::pair<Key, Value>* const item((std::pair<Key, Value>*)(top.m_payload & cpq_detail::Pointer_Mask));

			out = *item;

			return true;
		}
	}

	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(Value& out)
{
	std::pair<Key, Value> item;

	if (try_pop(item)) {
		out = item.second;
		return true;
	}

	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::reserve(size_type capacity)
{
	while (!m_items || m_items.load(std::memory_order_relaxed)->capacity() < capacity) {

		array_type* items(m_items.load(std::memory_order_relaxed));
		if (m_items) {
			array_type* last(items->last());

			size_type const currentCapacity(items->capacity());
			size_type const outstanding(currentCapacity < capacity ? capacity - currentCapacity : 0);

			if (outstanding) {
				array_type* const extension(make_array(outstanding, m_allocator));
				if (!last->try_extend(extension)) {
					destroy_array(extension, m_allocator);
				}
			}
		}
		else {
			array_type* exp(nullptr);
			array_type* const desired(make_array(capacity, m_allocator));
			if (!m_items.compare_exchange_weak(exp, desired)) {
				destroy_array(desired, m_allocator);
			}
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::size_type concurrent_priority_queue<Key, Value, Compare, Allocator>::capacity() const
{
	const array_type* items(m_items.load(std::memory_order_relaxed));
	return items ? items->capacity() : 0;
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::size_type concurrent_priority_queue<Key, Value, Compare, Allocator>::size() const
{
	return m_size.load(std::memory_order_relaxed);
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::unsafe_reserve(size_type capacity)
{
	if (m_items) {
		array_type* const old(m_items);
		size_type const oldCapacity(old->capacity());

		if (!(oldCapacity < capacity) && old->contiguous()) {
			return;
		}

		size_type const newCapacity(oldCapacity < capacity ? capacity : oldCapacity);

		array_type* const _new(make_array(newCapacity, m_allocator));

		copy_to(*_new, *old);

		destroy_array(old, m_allocator);

		m_items = _new;
	}
	else {
		m_items = make_array(capacity, m_allocator);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::unsafe_consolidate()
{
	array_type* const old(m_items);

	if (old) {
		if (old->contiguous()) {
			return;
		}

		array_type* const _new(make_array(old->capacity(), m_allocator));

		copy_to(*_new, *old);

		destroy_array(old, m_allocator);

		m_items = _new;
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline typename concurrent_priority_queue<Key, Value, Compare, Allocator>::array_type* concurrent_priority_queue<Key, Value, Compare, Allocator>::make_array(size_type capacity, allocator_type allocator) const
{
	const size_type itemsSize(sizeof(node_type) * capacity);
	const size_type headerSize(sizeof(array_type));

	std::uint8_t* const block(allocator.allocate(headerSize + itemsSize));

	std::uint8_t* const headerBegin(block);
	std::uint8_t* const itemsBegin(headerBegin + headerSize);

	array_type* const header(new (headerBegin) array_type(capacity));
	node_type* const items((node_type*)itemsBegin);

	std::uninitialized_value_construct(items, items + capacity);

	return header;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::destroy_array(array_type* arr, allocator_type allocator) const
{
	assert(arr && "Unexpected null pointer");

	array_type* prev(arr);
	do {
		std::uint8_t* const block((std::uint8_t*)prev);
		size_type const blockSize(prev->local_capacity() * sizeof(node_type) + sizeof(array_type));

		prev = prev->next();

		allocator.deallocate(block, blockSize);
	} while (prev);
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::copy_to(array_type& to, const array_type& from) const
{
	size_type const fromCapacity(from.capacity());

	assert(!(to.capacity() < fromCapacity) && "Target capacity low");

	for (size_type i = 0; i < fromCapacity; ++i) {
		to[i] = from[i].load();
	}
}

namespace cpq_detail {
static item_pack supplant_top(item_pack with, array_type& inArray, std::atomic<size_type>& size)
{
	// Hmm. So. Perhaps we need to know if children are living or not. Seems unfeasible to
	// trickle based on possibly outdated size data.

	// So we would LIKE for consumed children to be null..
	return item_pack();
}
static void bubble(size_type index, array_type& inArray)
{
	// Here, it should be easier to know. 
	// The item will be tagged, and no other
}
template <class Key, class Value>
union node
{
	const std::pair<Key, Value>* const m_item;
	std::atomic<uint64_t> m_ival;
};
template <class Key, class Value>
struct pop_operation
{
	// Conflict of pop and push repair operations means that, in the event of consumer holding parent
	// and producer holding [any] child, there will be a definite order things will have to happen in.
	// Note some facts:

	// * if the bubbling node is the lesser of the siblings that means it will be naturally shifted up
	// * if the bubbling node is the greater of the siblings that means in the case it's sibling is shifted up, 
	// it will *NOT* be shifted up (since the sibling is less)
	// * if the bubbling node is the greater of the siblings that means in the case it's sibling is NOT shifted up,
	// it will *NOT* be shifted up (since the parent will be less than the sibling)

	// * This means, in case of conflict, everything may proceed naturally, from the point of view of the consumer (since that 
	// always has two different ways to go)

	// So. Order of operations in case of conflict is
	// From consumer point of view - Unchanged
	// From producer point of view - In case parent is flagged do two things:
	// 1. Help evaluate min child
	// 2. In case child is sibling, abort
	//	  In case child is producer node, continue with own operations...... 


	// Idea: keep tags on pointers as they move up/down heap to know direction they are moving.... and perhaps have consumer / producer shift roles in case of collision 
	// (what about consumer/consumer or producer/producer in this case)

	// So consumer, consumer collision ?
	// producer, producer collision ?

	// Something like: try_advance_operation(op)

	// So, in case of collision -> try_advance_operation(blockingOp)


	// Aaand, if !try_advance_operation, perhaps we may let another thread complete it for us?


	std::atomic<std::pair<Key, Value>*> m_item;

	std::atomic<node<Key, Value>*> m_parent;
	std::atomic<node<Key, Value>*> m_left;
	std::atomic<node<Key, Value>*> m_right;
};



}
}

#pragma warning(pop)