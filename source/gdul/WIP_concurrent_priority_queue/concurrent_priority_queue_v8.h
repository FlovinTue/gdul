#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <thread>
#include <cassert>
#include <utility>
#include <random>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

namespace gdul {
namespace cpq_detail {

static constexpr std::uintptr_t Bottom_Bits = 1 << 1 << 1;
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Bottom_Bits;
static constexpr std::uintptr_t Version_Step = std::uintptr_t(1) << 48;
static constexpr std::uint8_t Cache_Line_Size = 64;

using size_type = std::size_t;

constexpr size_type log2ceil(size_type value)
{
	size_type highBit(0);
	size_type shift(value);
	for (; shift; ++highBit) {
		shift >>= 1;
	}

	highBit -= static_cast<bool>(highBit);

	const size_type mask((size_type(1) << (highBit)) - 1);
	const size_type remainder((static_cast<bool>(value & mask)));

	return highBit + remainder;
}
constexpr std::uint8_t calc_max_height(size_type maxEntriesHint)
{
	const std::uint8_t naturalHeight((std::uint8_t)log2ceil(maxEntriesHint));
	const std::uint8_t croppedHeight(naturalHeight / 2 + bool(naturalHeight % 2));

	return croppedHeight;
}
static std::uint8_t random_height();

static constexpr size_type Max_Entries_Hint = 1024;
static constexpr std::uint8_t Max_Node_Height = 2;// cpq_detail::calc_max_height(Max_Entries_Hint);

template <class Key, class Value>
struct node
{
	node() : m_kv(), m_next{}, m_height(0) {}
	node(std::pair<Key, Value>&& item) : m_kv(std::move(item)), m_next{}, m_height(0) {}
	node(const std::pair<Key, Value>& item) : m_kv(item), m_next{}, m_height(0){}


	struct node_view
	{
		node_view() = default;
		node_view(std::uintptr_t value) :m_value(value) {}
		node_view(node* n) : m_value((std::uintptr_t)(n)) {}

		operator node* ()
		{
			return (node*)(m_value & Pointer_Mask);
		}
		operator const node* () const
		{
			return (const node*)(m_value & Pointer_Mask);
		}

		operator std::uintptr_t() = delete;

		bool operator ==(node_view other) const
		{
			return operator const node * () == other.operator const node * ();
		}
		bool operator !=(node_view other) const
		{
			return !operator==(other);
		}
		std::uint32_t get_version() const
		{
			return m_version;
		}
		void set_version(std::uint32_t v)
		{
			m_version = v;
		}
		union
		{
			const node* m_nodeView;

			std::uintptr_t m_value;
			struct
			{
				std::uint16_t padding[3];
				std::uint16_t m_version;
			};
		};
	};
	union node_rep
	{
		node_rep() : m_value(0) {}
		node_rep(std::uintptr_t value) : m_value(value) {}
		node_rep(const node_view& view) : m_value(view.m_value) {}

		inline node_view load(std::memory_order m = std::memory_order_seq_cst) const
		{
			return m_value.load(m);
		}
		inline node_view exchange(node_view with)
		{
			return m_value.exchange(with.m_value, std::memory_order_relaxed);
		}

		const node* const m_nodeView;
		std::atomic<std::uintptr_t> m_value;
	};

	struct alignas(Cache_Line_Size) // Sync block
	{
		node_rep m_next[Max_Node_Height];
		std::uint8_t m_height;
#if defined (_DEBUG)
		std::uint8_t m_removed = 0;
		std::uint8_t m_inserted = 0;
#endif
	};

	std::pair<Key, Value> m_kv;
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

	void push(node_type* node);
	bool try_pop(node_type*& out);

	void clear(); // poorly implemented, not for concurrent use yet

	bool empty() const noexcept;

private:
	using node_rep = typename cpq_detail::node<Key, Value>::node_rep;
	using node_view = typename cpq_detail::node<Key, Value>::node_view;
	using node_view_set = node_view[cpq_detail::Max_Node_Height];

	void push_internal(node_type* node);

	bool prepare_insertion_sets(node_view_set& outCurrent, node_view_set& outNext, node_type* node);
	bool verify_head_set(const node_view_set& set, std::uint8_t height) const;

	void unlink_successors(node_type* of, const node_type* expecting);

	static bool try_link(node_type* node, node_view_set& current, node_view_set& next);

	// Also end
	node_type m_head;

	allocator_type m_allocator;
	compare_type m_comparator;
};


// States of a link set:
// 1. Partially partially de-linked
// 2. Partially linked
// 3. Linked
// 4. Partially de-linked & Partially linked


template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue()
	: m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();
	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[i].m_value.store((std::uintptr_t) & m_head);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::~concurrent_priority_queue()
{
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	node->m_height = cpq_detail::random_height();

	push_internal(node);
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
	node_view_set frontSet;
	node_view_set replacementSet;

	do {
		for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
			const std::uint8_t atLayer(cpq_detail::Max_Node_Height - 1 - i);

			frontSet[atLayer] = m_head.m_next[atLayer].load(std::memory_order_seq_cst);
		}

		node_type* const frontNode(frontSet[0]);
		if (frontNode == &m_head) {
			return false;
		}

		const std::uint8_t frontHeight(frontNode->m_height);

		for (std::uint8_t i = 0; i < frontHeight; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);


			// These values should never be the same as head values when using single consumer..
			// So we're ending up with a front node linking itself in upper layer?? 
			replacementSet[atLayer] = frontNode->m_next[atLayer].load();
			replacementSet[atLayer].set_version(frontSet[atLayer].get_version() + 1);
		}


		// So if blocking inserters from 
		for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);

			// What about HEAD / END here? This causes positive test. Probably not desirable. Or is it? Might not matter.
			if (frontSet[atLayer] == replacementSet[atLayer] &&
				frontSet[atLayer].operator node_type * () != &m_head) {
				continue;
			}
			if (m_head.m_next[atLayer].m_value.compare_exchange_strong(frontSet[atLayer].m_value, replacementSet[atLayer].m_value, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				replaced = frontSet[atLayer];
				replacedWith = replacementSet[atLayer];
				continue;
			}
			// Retry if an inserter just linked front node
			if (frontSet[atLayer].operator node_type * () == frontNode) {
				if (m_head.m_next[atLayer].m_value.compare_exchange_strong(frontSet[atLayer].m_value, replacementSet[atLayer].m_value, std::memory_order_seq_cst, std::memory_order_relaxed)) {
					replaced = frontSet[atLayer];
					replacedWith = replacementSet[atLayer];
					continue;
				}
			}
		}
	} while (!m_head.m_next[0].m_value.compare_exchange_weak(frontSet[0].m_value, replacementSet[0].m_value, std::memory_order_seq_cst, std::memory_order_relaxed));

	node_type* const claimed(frontSet[0]);


#if defined (_DEBUG)
	claimed->m_removed = 1;
#endif

	unlink_successors(claimed, (node_type*)replacementSet[0]);

	out = claimed;


	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::clear()
{
	// Perhaps just call this unsafe_clear..

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[cpq_detail::Max_Node_Height - i - 1].m_value.store((std::uintptr_t) & m_head);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::empty() const noexcept
{
	return m_head.m_next[0].load() == &m_head;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link(node_type* node, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outCurrent, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outNext)
{
	const std::uint8_t nodeHeight(node->m_height);

	for (size_type i = 0; i < nodeHeight; ++i) {
		node->m_next[i].m_value.store(outNext[i].m_value);
	}

	const std::uintptr_t desired((std::uintptr_t)node);

	node_type* const baseLayerCurrent(outCurrent[0].operator node_type * ());

	// The base layer is what matters. The rest is just search-juice
	{
		std::uintptr_t expected(outNext[0].m_value);
		if (!baseLayerCurrent->m_next[0].m_value.compare_exchange_weak(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			return false;
		}
		node->m_inserted = 1;
	}

	for (size_type layer = 1; layer < nodeHeight; ++layer) {
		std::uintptr_t expected(outNext[layer].m_value);
		if (!outCurrent[layer].operator node_type * ()->m_next[layer].m_value.compare_exchange_weak(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			break;
		}
	}

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push_internal(node_type* node)
{
	node_view_set current{};
	node_view_set next{};

	for (;;) {
		if (prepare_insertion_sets(current, next, node) &&
			try_link(node, current, next)) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::prepare_insertion_sets(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outCurrent, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outNext, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	const node_view head(&m_head);
	std::uninitialized_fill(std::begin(outCurrent), std::end(outCurrent), head);

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const size_type atLayer(cpq_detail::Max_Node_Height - i - 1);

		for (;;){
			// !!--------------------------!!
			// What about patching the layers separately? 
			// Treating all lanes as a separate list ?
			// !!--------------------------!!


			node_type* const currentNode(outCurrent[atLayer]);
			outNext[atLayer] = node_view(currentNode->m_next[atLayer].m_value.load(std::memory_order_relaxed));
			node_type* const nextNode(outNext[atLayer]);

			if (!nextNode) {
				return false;
			}

			if (!m_comparator(nextNode->m_kv.first, node->m_kv.first)) {
				break;
			}

			outCurrent[atLayer] = outNext[atLayer];
		}

		if (atLayer) {
			outCurrent[atLayer - 1] = outCurrent[atLayer];
		}
	}

	// Make sure we're not carrying any stale links in upper layers
	if (outCurrent[0].operator node_type * () == &m_head) {
		return verify_head_set(outNext, node->m_height);
	}

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::verify_head_set(const typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& set, std::uint8_t height) const
{
	for (std::uint8_t i = 1; i < height; ++i) {
		const node_type* const expected(set[i]);
		const node_type* const actual(m_head.m_next[i].load());
		if (actual != expected) {
			return false;
		}
	}
	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::unlink_successors(node_type* of, const node_type* expecting)
{
	node_type* successor(of->m_next[0].exchange(nullptr));

	// Scan forward until we find the original node. (It has been supplanted by recent insertions)

	while (successor != expecting) {
		node_type* const toReinsert(successor);
		successor = successor->m_next[0].exchange(nullptr);
		toReinsert->m_inserted = 0;
		
#if defined _DEBUG
		for (std::uint8_t i = 1; i < toReinsert->m_height; ++i) {
			toReinsert->m_next[i].m_value.store(0, std::memory_order_relaxed);
		}
#endif

		push_internal(toReinsert);
	}
}


namespace cpq_detail {
struct tl_container
{
	// https://codingforspeed.com/using-faster-psudo-random-generator-xorshift/

	std::uint32_t x = 123456789;
	std::uint32_t y = 362436069;
	std::uint32_t z = 521288629;
	std::uint32_t w = 88675123;

	std::uint32_t operator()()
	{
		std::uint32_t t;
		t = x ^ (x << 11);
		x = y; y = z; z = w;
		return w = w ^ (w >> 19) ^ (t ^ (t >> 8));
	}

	tl_container()
		: x(123456789)
		, y(362436069)
		, z(521288629)
		, w(88675123)
	{
		std::random_device rd;

		// randomize starting point
		for (std::size_t i = rd() % 100; i != 0; --i) {
			t_rng();
		}
	}

}static thread_local t_rng;

static thread_local size_type testCount;
static std::uint8_t random_height()
{
	std::uint8_t result(1);

	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height - 1; ++i, ++result) {
		if ((t_rng() % 4) != 0) {
			break;
		}
	}

	return result;

}
}
}

#pragma warning(pop)
