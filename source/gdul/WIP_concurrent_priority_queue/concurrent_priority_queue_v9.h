#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <thread>
#include <cassert>
#include <utility>
#include <random>
#include "../Testers/Common/util.h"

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

namespace gdul {
namespace cpq_detail {

static constexpr std::uintptr_t Bottom_Bits = (((1 << 1) | 1) << 1) | 1;
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Bottom_Bits;
static constexpr std::uintptr_t Version_Mask = ~Pointer_Mask;
static constexpr std::uint32_t Max_Version = std::numeric_limits<std::uint16_t>::max() * 2 * 2 * 2;
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
		node_view(node* n) : m_value((std::uintptr_t)(n)) {}
		node_view(node* n, std::uint32_t version) :node_view(n) { set_version(version); }

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
			const std::uint64_t pointerValue(m_value & Version_Mask);
			const std::uint64_t lower(pointerValue & Bottom_Bits);
			const std::uint64_t upper(pointerValue >> 48);
			const std::uint64_t conc(lower + upper);

			return (std::uint32_t)conc;
		}
		void set_version(std::uint32_t v)
		{
			const std::uint64_t  v64(v);
			const std::uint64_t upper((v64 & ~Bottom_Bits) << 48);
			const std::uint64_t lower(v64 & Bottom_Bits);
			const std::uint64_t mask(upper | lower);

			const std::uint64_t pointerValue(m_value & Pointer_Mask);

			m_value = (mask | pointerValue);
		}
		union
		{
			const node* m_nodeView;
			std::uintptr_t m_value;
		};
	};

	struct alignas(Cache_Line_Size) // Sync block
	{
		std::atomic<node_view> m_next[Max_Node_Height];
		std::uint8_t m_height;
#if defined (_DEBUG)
		const node* m_nextView[Max_Node_Height]{};
		std::uint32_t m_version = 0;
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

	size_type size() const
	{
		size_type s(0);
		for (const node_type* at(m_head.m_next[0].load()); at != &m_head; ++s, at = at->m_next[0].load());
		return s;
	}

private:
	using node_view = typename cpq_detail::node<Key, Value>::node_view;
	using node_view_set = node_view[cpq_detail::Max_Node_Height];

	bool find_node(const node_type* node) const;
	void prepare_insertion_sets(node_view_set& outCurrent, node_view_set& outNext, const node_type* node);

	bool at_end(const node_type*) const;

	bool try_flag_deletion(node_type* at, node_view& replacement, std::uint32_t version, bool& flagged);

	bool try_push(node_type* node);

	bool try_link(node_view_set& current, node_view_set& next, node_type* node);
	void try_link_upper(node_view_set& current, node_view_set& next, node_view node);
	bool try_swap_front(node_view_set& expectedFront, node_view_set& desiredFront);
	bool try_splice_at_front(node_view_set& current, node_view_set& next, node_view node);

	void load_front_set(node_view_set& outSet) const;


	// Also end
	node_type m_head;

	allocator_type m_allocator;
	compare_type m_comparator;
};

template<class Key, class Value, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, Compare, Allocator>::concurrent_priority_queue()
	: m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();
	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[i].store(&m_head);
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

	while (!try_push(node));
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
	node_view_set frontSet;
	node_view_set replacementSet;

	bool flagged(false);
	bool deLinked(false);

	node_type* frontNode(nullptr);

	do {
		load_front_set(frontSet);

		frontNode = frontSet[0];

		if (at_end(frontNode)) {
			return false;
		}

		const std::uint8_t frontHeight(frontNode->m_height);

		for (std::uint8_t i = 0; i < frontHeight; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);

			// These values should never be the same as head values when using single consumer..
			// So we're ending up with a front node linking itself in upper layer?? 
			replacementSet[atLayer] = frontNode->m_next[atLayer].load();
		}

		if (!try_flag_deletion(frontNode, replacementSet[0], frontSet[0].get_version() + 1, flagged)) {
			continue;
		}

		deLinked = try_swap_front(frontSet, replacementSet);

		if (flagged && !deLinked) {
			const node_type* const actualFront(frontSet[0]);
			const node_type* const triedReplacement(replacementSet[0]);

			// This is convoluted. find_node...... 
			deLinked = (actualFront == triedReplacement) || !find_node(frontNode);
		}

	} while (!flagged || !deLinked);

	out = frontNode;

#if defined _DEBUG
	frontNode->m_removed = 1;
	frontNode->m_version = frontNode->m_next[0].load(std::memory_order_relaxed).get_version();
	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		frontNode->m_nextView[i] = frontNode->m_next[i].load(std::memory_order_relaxed);
	}
#endif

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::prepare_insertion_sets(node_view_set& outCurrent, node_view_set& outNext, const node_type* node)
{
	outCurrent[cpq_detail::Max_Node_Height - 1] = &m_head;

	const key_type key(node->m_kv.first);

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const size_type atLayer(cpq_detail::Max_Node_Height - i - 1);

		for (;;) {
			node_type* const currentNode(outCurrent[atLayer]);
			outNext[atLayer] = node_view(currentNode->m_next[atLayer].load(std::memory_order_relaxed));
			node_type* const nextNode(outNext[atLayer]);

			if (!m_comparator(nextNode->m_kv.first, key)) {
				break;
			}

			outCurrent[atLayer] = nextNode;
		}

		if (atLayer) {
			outCurrent[atLayer - 1] = outCurrent[atLayer];
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_flag_deletion(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& replacement, std::uint32_t version, bool& flagged)
{
	flagged = false;

	// Small chance of loop around and deadlocking
	if (replacement.get_version() != version) {

		const node_view desired(replacement, version);

		if (flagged = at->m_next[0].compare_exchange_weak(replacement, desired)) {
			replacement = desired;
		}

		const std::uint32_t foundVersion(replacement.get_version());
		if (foundVersion != version) {
			// This thread probably stalled and other things have happened with this node..
			return false;
		}
	}
	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_swap_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedFront, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& desiredFront)
{
	// De-link upper layers from head
	const node_type* const frontNode(expectedFront[0]);
	const std::uint8_t frontHeight(frontNode->m_height);

	for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
		const std::uint8_t atLayer(frontHeight - 1 - i);

		desiredFront[atLayer].set_version(expectedFront[atLayer].get_version() + 1);

		if (m_head.m_next[atLayer].compare_exchange_strong(expectedFront[atLayer], desiredFront[atLayer], std::memory_order_seq_cst, std::memory_order_relaxed)) {
			continue;
		}

		// Retry if an inserter just linked front node to an upper layer..
		if (expectedFront[atLayer].operator node_type * () == frontNode) {
			if (m_head.m_next[atLayer].compare_exchange_strong(expectedFront[atLayer], desiredFront[atLayer], std::memory_order_seq_cst, std::memory_order_relaxed)) {
				continue;
			}
		}
	}

	// De-link base layer
	desiredFront[0].set_version(expectedFront[0].get_version());
	const bool deLinked = m_head.m_next[0].compare_exchange_strong(expectedFront[0], desiredFront[0], std::memory_order_seq_cst, std::memory_order_relaxed);

	return deLinked;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::clear()
{
	// Perhaps just call this unsafe_clear..

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[cpq_detail::Max_Node_Height - i - 1].store(&m_head);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::empty() const noexcept
{
	return at_end(m_head.m_next[0].load());
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& current, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	const std::uint8_t nodeHeight(node->m_height);

	for (size_type i = 0; i < nodeHeight; ++i) {
		node->m_next[i].store(next[i], std::memory_order_relaxed);
	}

	node_type* const baseLayerCurrent(current[0].operator node_type * ());

	node_view expected(next[0]);
	const node_view desired(node);

	GDUL_ASSERT(!baseLayerCurrent->m_removed && "Should not link to deleted node");

	if (!baseLayerCurrent->m_next[0].compare_exchange_weak(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
		return false;
	}
#if defined _DEBUG
	node->m_inserted = 1;
#endif

	//try_link_upper(current, next, node);

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_upper(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& current, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	const std::uint8_t height(node.operator node_type*()->m_height);

	for (size_type layer = 1; layer < height; ++layer) {
		node_view expected(next[layer]);
		const node_view desired(node);
		if (!current[layer].operator node_type * ()->m_next[layer].compare_exchange_weak(expected, node_view(node), std::memory_order_seq_cst, std::memory_order_relaxed)) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_splice_at_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& current, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	next[0] = node;

	if (try_swap_front(current, next)) {

		node_view_set headSet;

		auto headBegin(std::begin(headSet));
		++headBegin;
		auto headEndItr(std::end(headSet));
		headEndItr += node.operator node_type * ()->m_height;

		std::uninitialized_fill(headBegin, headEndItr, node_view(&m_head));
		std::for_each(headBegin, headEndItr, [this, &next](node_view& n) {n = &m_head; n.set_version(next[0].get_version() + 1); });

		try_link_upper(headSet, next, node);

		return true;
	}

	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::load_front_set(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outSet) const
{
	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const std::uint8_t atLayer(cpq_detail::Max_Node_Height - 1 - i);

		outSet[atLayer] = m_head.m_next[atLayer].load(std::memory_order_seq_cst);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	node_view_set currentSet;
	node_view_set nextSet;

	prepare_insertion_sets(currentSet, nextSet, node);

	// Empty structure. All safe.
	if (at_end(currentSet[0]) && at_end(nextSet[0])) {
		return try_link(currentSet, nextSet, node);
	}

	// The node has no version, which means it is not in the process of deletion. All safe.
	if (!nextSet[0].get_version()) {
		return try_link(currentSet, nextSet, node);
	}

	// Here ensure that we're not linking to a node that's being removed. 
	// If it has version set, it means one of three things:
	// * The node is at front, in the middle of deletion
	// * The node has been supplanted, and is no longer at front
	// * The node has been deleted, and does not exist in structure

	// The node is at front. We want to help de-link the node, replacing it with our own. This means we need a special case
	// where currentSet is HEAD, nextSet is FRONTNEXT and expected is FRONT
	{
		node_view_set frontSet;
		load_front_set(frontSet);

		if (frontSet[0] == currentSet[0] && nextSet[0].get_version() == frontSet[0].get_version() + 1) {

			return try_splice_at_front(currentSet, nextSet, node);
		}
	}

	// The node has version set (was claimed) but is not at front. We need to know if it's in the list still.
	if (find_node(currentSet[0])) {
		return try_link(currentSet, nextSet, node);
	}

	// The node is removed, retry
	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::find_node(const typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node) const
{
	//!!-----------------------------------------------------!!
	// For this to not be ambiguous, de-linking needs to be guaranteed. That is, there may be *NO* remaining links in the
	// list when a node is de-linked at bottom layer.
	//!!-----------------------------------------------------!!


	// Might also be able to take a source node argument, so as to skip loading head here.. 

#if defined DYNAMIC_SCAN
	const key_type k(node->m_kv.first);

	const std::uint8_t topLayer(cpq_detail::Max_Node_Height - 1);

	const node_type* at(m_head.m_next[topLayer].load());

	// Descend and scan forward
	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const std::uint8_t layer(topLayer - i);

		for (;;) {
			if (at_end(at)) {
				break;
			}

			if (m_comparator(k, at->m_kv.first)) {
				break;
			}

			if (at == node) {
				return true;
			}

			at = at->m_next[layer].load();
		}
	}

	// Scan possible duplicate keys
	for (;;) {
		if (at_end(at)) {
			break;
		}

		if (m_comparator(k, at->m_kv.first)) {
			break;
		}

		if (at == node) {
			return true;
		}

		at = at->m_next[0].load();
}

	return false;

#else // Linear base layer scan

	const key_type k(node->m_kv.first);
	const node_type* at(m_head.m_next[0].load());

	for (;;) {
		if (at_end(at)) {
			break;
		}

		if (m_comparator(k, at->m_kv.first)) {
			break;
		}

		if (at == node) {
			return true;
		}

		at = at->m_next[0].load();
	}

	return false;
#endif
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::at_end(const concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* n) const
{
	return n == &m_head;
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



// Current state is

// for push:
// * Compile insertion set. - This may contain any nodes, both living or dead nodes
// * Ensure bottom node in insertion set is still living. 
// * Check which insertion case we are dealing with: At main list body or after an ongoing de-link (at FRONT)

// * If inserting at main list, simply attempt to link in.
// * If inserting after an ongoing de-link (FRONT), perform a combined help_delink along with linking node at HEAD

// The main point of uncertainty is linkage of upper layers. Linking should be blocked at upper layers at front node, in case
// the ongoing linked node has been removed. -- The mechanism is complex though, and there is a big chance of unforseen things 
// happening.



// for pop:
// * Load HEAD set, top to bottom
// * Try flag FRONT node
// * Help de-link FRONT
// * In case flag was successful, abort. Else repeat.