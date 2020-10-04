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

	void push_internal(node_type* node);

	bool find_node(const node_type* node) const;

	bool at_end(const node_type*) const;

	bool try_flag_deletion(node_view n, node_view& replacement);
	bool try_swap_front(node_view_set& expectedFront, const node_view_set& desiredFront);

	bool prepare_insertion_sets(node_view_set& outCurrent, node_view_set& outExpectedNext, node_view_set& outNext, node_type* node);

	bool try_link(node_view_set& current, node_view_set& expectedNext, node_view_set& next, node_type* node);

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

	push_internal(node);
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
	node_view_set frontSet;
	node_view_set replacementSet;

	bool flagged(false);
	bool deLinked(false);

	node_type* frontNode(nullptr);

#if defined _DEBUG
	int delinkCode(0);

	node_type* frontNodes[cpq_detail::Max_Node_Height];

	node_view frontNext(nullptr);
#endif

	do {
		for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
			const std::uint8_t atLayer(cpq_detail::Max_Node_Height - 1 - i);

			frontSet[atLayer] = m_head.m_next[atLayer].load(std::memory_order_seq_cst);

#if defined _DEBUG
			frontNodes[atLayer] = frontSet[atLayer];
#endif
		}

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


		// Flag front for deletion

		const std::uint32_t headVersion(frontSet[0].get_version());
		const std::uint32_t nextVersion(headVersion + 1);

		{
			flagged = false;
			// Small chance of loop around and deadlocking
			if (replacementSet[0].get_version() != nextVersion) {
				node_type* const front(frontSet[0]);

				const node_view desired(replacementSet[0], nextVersion);

				if (flagged = front->m_next[0].compare_exchange_weak(replacementSet[0], desired)) {
					replacementSet[0] = desired;
				}

				const std::uint32_t foundVersion(replacementSet[0].get_version());
				if (foundVersion != nextVersion) {
					// This thread probably stalled and other things have happened with this node..
					continue;
				}
			}
		}

#if defined _DEBUG
		node_view_set frontCopy;
		std::copy(std::begin(frontSet), std::end(frontSet), frontCopy);
#endif
		for (std::uint8_t i = 0; i < frontHeight; ++i) {
			replacementSet[i].set_version(nextVersion);
		}

		deLinked = try_swap_front(frontSet, replacementSet);

		if (flagged && !deLinked) {
			const node_type* const actualFront(frontSet[0]);
			const node_type* const triedReplacement(replacementSet[0]);

#if defined _DEBUG
			const std::uint32_t myFlaggedVersion(replacementSet[0].get_version());
#endif
			// This is convoluted. find_node...... 
			deLinked = (actualFront == triedReplacement) || !find_node(frontNode);
#if defined _DEBUG
			if (actualFront == triedReplacement) {
				GDUL_ASSERT(false);
				delinkCode = 2;
			}
			else if (deLinked){
				delinkCode = 3;
				GDUL_ASSERT(false);
			}
			const node_view frontBase(m_head.m_next[0].load());
			const node_type* const nodePtr(frontBase);
			const std::uint32_t flaggedVersion(frontBase.get_version());
#endif
		}

	} while (!flagged || !deLinked);



#if defined (_DEBUG)
	node_type* const claimed(frontNode);
	// At this point, claimed should also not have any new nodes linked to it.. 
	//GDUL_ASSERT(claimed->m_next[0].load() == frontNext);
	GDUL_ASSERT(!find_node(claimed));
	claimed->m_removed = 1;
#endif

	out = frontNode;

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_flag_deletion(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view n, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& replacement)
{
	const std::uint32_t headVersion(n.get_version());
	const std::uint32_t nextVersion(headVersion + 1);

	// Small chance of loop around and deadlocking
	if (replacement.get_version() == nextVersion) {
		return false;
	}

	node_type* const front(n);

	const node_view desired(replacement, nextVersion);
	const bool flagged(front->m_next[0].compare_exchange_weak(replacement, desired));

	if (flagged) {
		replacement.set_version(nextVersion);
	}

	return flagged;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_swap_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedFront, const typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& desiredFront)
{
	// De-link upper layers from head
#if defined _DEBUG
	int replaced(0);
#endif
	// Seems like something funky here. Investigate..
	for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
		const std::uint8_t atLayer(frontHeight - 1 - i);

		//			if (expectedFront[atLayer].get_version() == nextVersion) {
		//#if defined _DEBUG
		//				replaced += 1;
		//#endif
		//				continue;
		//			}

		if (m_head.m_next[atLayer].compare_exchange_strong(expectedFront[atLayer], desiredFront[atLayer], std::memory_order_seq_cst, std::memory_order_relaxed)) {
#if defined _DEBUG
			replaced += 2;
#endif
			continue;
		}

		// Retry if an inserter just linked front node to an upper layer..
		if (expectedFront[atLayer].operator node_type * () == frontNode) {
			if (m_head.m_next[atLayer].compare_exchange_strong(expectedFront[atLayer], desiredFront[atLayer], std::memory_order_seq_cst, std::memory_order_relaxed)) {
#if defined _DEBUG
				replaced += 3;
#endif
				continue;
			}
		}
	}

#if defined _DEBUG
	frontNext = desiredFront[0];
#endif

	// De-link base layer
	const bool deLinked = m_head.m_next[0].compare_exchange_strong(expectedFront[0], desiredFront[0], std::memory_order_seq_cst, std::memory_order_relaxed);


#if defined _DEBUG

	//GDUL_ASSERT(!(at_end(m_head.m_next[0].load()) && !at_end(m_head.m_next[1].load())));

	if (deLinked) {
		//GDUL_ASSERT(frontNode->m_next[0].load() == frontNext);

		delinkCode = 1;

		for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);

			const node_type* loadedFront(m_head.m_next[atLayer].load());

			GDUL_ASSERT(replaced);

			GDUL_ASSERT(loadedFront != frontNode);
		}
	}
#endif

	return delinked;
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
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& current, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedNext, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	const std::uint8_t nodeHeight(node->m_height);

	for (size_type i = 0; i < nodeHeight; ++i) {
		node->m_next[i].store(next[i], std::memory_order_relaxed);
	}

	node_type* const baseLayerCurrent(current[0].operator node_type * ());

	{
		node_view desired(node);
		node_view expected(expectedNext[0]);
		if (!baseLayerCurrent->m_next[0].compare_exchange_weak(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			return false;
		}
#if defined _DEBUG
		node->m_inserted = 1;
#endif
	}


	for (size_type layer = 1; layer < nodeHeight; ++layer) {
		node_view expected(expectedNext[layer]);
		if (!current[layer].operator node_type * ()->m_next[layer].compare_exchange_weak(expected, node_view(node), std::memory_order_seq_cst, std::memory_order_relaxed)) {
			break;
		}
	}

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::push_internal(node_type* node)
{
	node_view_set current{};
	node_view_set expectedNext{};
	node_view_set next{};

	for (;;) {
		if (prepare_insertion_sets(current, expectedNext, next, node) &&
			try_link(current, expectedNext, next, node)) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::prepare_insertion_sets(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outCurrent, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outExpectedNext, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outNext, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
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


	// We're linking into an empty structure,  just go ahead..
	if (at_end(outNext[0])) {
		std::copy(std::begin(outNext), std::end(outNext), outExpectedNext);
		return true;
	}

	// Here ensure that we're not linking to a node that's being removed. 
	// If it has version set, it means one of three things:
	// * The node is at front, in the middle of deletion
	// * The node has been supplanted, and is no longer at front
	// * The node has been deleted, and does not exist in structure

	const std::uint32_t version(outNext[0].get_version());

	// The node has no version, which means it must be unclaimed. All safe.
	if (!version) {
		std::copy(std::begin(outNext), std::end(outNext), outExpectedNext);
		return true;
	}

	// The node is at front. We want to help de-link the node, replacing it with our own. This means we need a special case
	// where current is HEAD, next is FRONTNEXT and expected is FRONT

	// This is STILL wrong. This need to be handled as a special case. When de-linking front it needs to be executed in the same manner
	// as a pop operation...... So. Let's construct a generalized try_swap_front!
	{
		node_view_set frontSet;
		for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
			const std::uint8_t atLayer(cpq_detail::Max_Node_Height - 1 - i);

			frontSet[atLayer] = m_head.m_next[atLayer].load(std::memory_order_seq_cst); // Can probably optimize this somewhat to (usually) avoid loading head beforehand..
		}
		if (frontSet[0] == outCurrent[0]) {
			std::copy(std::begin(outCurrent), std::end(outCurrent), outExpectedNext);
			return true;
		}
	}

	// The node has version set (was claimed) but is not at front. We need to know if it's in the list still.
	if (find_node(outCurrent[0])) {
		std::copy(std::begin(outNext), std::end(outNext), outExpectedNext);
		return true;
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



// !!--------------------------!!
// What about patching the layers separately? 
// Treating all lanes as a separate list ?
// !!--------------------------!!
// Need some kind of progress guarantee?
// What about allowing deletion away from front?
// Linking from top? Perhaps? Conceptually, this would
// make a newly inserted node remain unlinked until base layer linkage.. What benefit would this yield?
// What disadvantages?

// Count references to each node? There *could* / *should* be a predictable set of them..

// Versioning in HEAD mirroring it in front?

// Probably upper layers will have the same issue as before.. : link to front, stall, delete, link to upper.. 

// But yeah.

// Head holds version 1

// cas front version into 2

// Inserter must know if the current node is the front node. It doesn't need to investigate this unless the
// current node has a version set.

// .. What of a de-linked node. How does that work? If it is not front node, why would other inserters not link there (owning their
// own stale open links to this node).... ? I suppose that might be a rare scenario, and we can simply attempt a search for the 
// node.. :) .. Maybe.

// One thing though: What about upper links to stale nodes? Will these cause trouble? Maybe. Yeah.. It has
// historically been very troublesome dealing with links to and from stale nodes.








	// Ok I think I got a new one: Inserters that want to insert at front->m_next will load value. In case the value
	// has version set it might mean there is an intention to delete this node. It then investigates if this node is the
	// front node. If this is NOT the case, it may link to it. If this IS the case it may take part in helping de-link at HEAD
	// while replacing it with it's own node.


	// Ok here is where we have to make changes. Special linking if we are dealing with front node.
	// It'll have to be sequenced very cleverly. We need to avoid any issues with stalling and then causing staleness
	// in a more recent insertion at front node. Ok inserters will have to clear version, that'll make their links distinguishable.