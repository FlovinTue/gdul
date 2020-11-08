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

#define GDUL_CPQ_DEBUG

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
	node() : m_kv(), m_next{}, m_height(Max_Node_Height) {}
	node(std::pair<Key, Value>&& item) : m_kv(std::move(item)), m_next{}, m_height(Max_Node_Height) {}
	node(const std::pair<Key, Value>& item) : m_kv(item), m_next{}, m_height(Max_Node_Height){}


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
#if defined (GDUL_CPQ_DEBUG)
		const node* m_nextView[Max_Node_Height]{};
		std::uint32_t m_delinkVersion = 0ul - 1;
		std::uint32_t m_linkVersion = 0ul - 1;
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
	bool at_head(const node_type*) const;

	bool try_flag_deletion(node_view front, node_view& replacement, bool& flagged);

	bool try_push(node_type* node);

	bool try_link(node_view_set& current, node_view_set& next, node_type* node);
	void try_link_upper(node_view_set& current, node_view_set& next, node_view node);
	bool try_swap_front(node_view_set& expectedFront, node_view_set& desiredFront);
	bool try_splice_to_head(node_view_set& current, node_view_set& next, node_type* node);

	bool cas_to_head(node_view& expected, node_view& desired, std::uint8_t layer);

	static void set_version(std::uint32_t version, node_view_set& to, std::uint8_t usingHeight);
	static void load_set(node_view_set& outSet, node_type* at, std::uint8_t offset = 0);


#if defined GDUL_CPQ_DEBUG
	struct expanded_node_view
	{
		expanded_node_view() = default;
		expanded_node_view(node_view v)
		{
			n = v;
			ver = v.get_version();
		}
		const node_type* n;
		std::uint32_t ver;
	};
	struct recent_op_info
	{
		expanded_node_view expected[cpq_detail::Max_Node_Height];
		expanded_node_view expected2[cpq_detail::Max_Node_Height];
		expanded_node_view actual[cpq_detail::Max_Node_Height];
		expanded_node_view next[cpq_detail::Max_Node_Height];

		const node_type* opNode = nullptr;

		bool result[cpq_detail::Max_Node_Height];
		bool frontSwap;

		uint32_t m_sequenceIndex = 0;
		uint8_t m_insertionCase = 0;
		std::uint8_t m_threadRole = 0;
		bool m_invertedLoading = false;
	};
	inline static thread_local recent_op_info t_recentOp = recent_op_info();
	inline static recent_op_info t_nonsense = recent_op_info();
	inline static std::vector<recent_op_info> t_recentPushes;
	inline static std::vector<recent_op_info> t_recentPops;
	inline static thread_local std::uint8_t t_threadRole = 0;

	std::atomic<uint32_t> m_sequenceCounter = 0;
#endif

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
#if defined GDUL_CPQ_DEBUG
	t_threadRole = 2;
#endif
	while (!try_push(node));

#if defined (GDUL_CPQ_DEBUG)
	t_recentPushes.push_back(t_recentOp);
#endif
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
#if defined GDUL_CPQ_DEBUG
	t_threadRole = 1;
#endif

	node_view_set frontSet;
	node_view_set replacementSet;

	bool flagged(false);
	bool deLinked(false);

	node_type* frontNode(nullptr);

	do {
		load_set(frontSet, &m_head);

		frontNode = frontSet[0];

		if (at_end(frontNode)) {
			return false;
		}

		load_set(replacementSet, frontNode);

		// What about this, succeeding flag here... Perhaps. Yeah. So in case we load entire replacement set,
		// we can stall. Then a new node is linked at base. .Hmm No. That'll still work.
		if (!try_flag_deletion(frontSet[0], replacementSet[0], flagged)) {
			continue;
		}

		deLinked = try_swap_front(frontSet, replacementSet);

		if (flagged && !deLinked) {
			const node_type* const actualFront(frontSet[0]);
			const node_type* const triedReplacement(replacementSet[0]);

			deLinked = (actualFront == triedReplacement) || !find_node(frontNode);
		}

	} while (!flagged || !deLinked);

	out = frontNode;

#if defined GDUL_CPQ_DEBUG
	frontNode->m_removed = 1;
	frontNode->m_delinkVersion = frontNode->m_next[0].load(std::memory_order_relaxed).get_version();
	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_nextView[i] = replacementSet[i];
	}
	std::copy(std::begin(frontSet), std::end(frontSet), t_recentOp.actual);
	std::copy(std::begin(replacementSet), std::end(replacementSet), t_recentOp.next);
	t_recentOp.opNode = frontNode;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
#endif

#if defined (GDUL_CPQ_DEBUG)
	t_recentPops.push_back(t_recentOp);
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

	// In case we are inserting at front, we need to make sure that the age of
	// our view of the upper layers do not exceed that of the base layer. (Lest we might refer to
	// a stale node in upper layers when succeeding a base layer link)
#if defined (GDUL_CPQ_DEBUG)
	t_recentOp.m_invertedLoading = false;
#endif
	if (at_head(outCurrent[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_invertedLoading = true;
#endif
		load_set(outNext, &m_head, 1);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_flag_deletion(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view front, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& replacement, bool& flagged)
{
	flagged = false;

	const std::uint32_t frontVersion(front.get_version());
	const std::uint32_t flagVersion(frontVersion + 1);

	// Small chance of loop around and deadlocking
	if (replacement.get_version() != flagVersion) {

		node_type* const frontNode(front);

		const node_view desired(replacement, flagVersion);

		if (flagged = frontNode->m_next[0].compare_exchange_strong(replacement, desired, std::memory_order_relaxed)) {
			replacement = desired;
		}

		const std::uint32_t foundVersion(replacement.get_version());
		if (foundVersion != flagVersion) {
			// This thread probably stalled and other things have happened with this node..
			return false;
		}
	}
	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_swap_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedFront, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& desiredFront)
{
#if defined GDUL_CPQ_DEBUG
	expanded_node_view expectedCopy[cpq_detail::Max_Node_Height];
	std::copy(std::begin(expectedFront), std::end(expectedFront), expectedCopy);

	bool performStore = false;
#endif

	const node_type* const frontNode(expectedFront[0]);
	const std::uint8_t frontHeight(frontNode->m_height);

	for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
		const std::uint8_t atLayer(frontHeight - 1 - i);

		// De-link upper layer
		if (cas_to_head(expectedFront[atLayer], desiredFront[atLayer], atLayer)) {
#if defined GDUL_CPQ_DEBUG
			t_recentOp.expected2[atLayer] = node_view();
			if (desiredFront[atLayer].get_version() == 5) {
				performStore = true;
			}
#endif
			continue;
		}

#if defined GDUL_CPQ_DEBUG
		t_recentOp.expected2[atLayer] = expectedFront[atLayer];
#endif

		// So what about this? What are the edge cases? If an insertion happens, followed by a delete,
		// this might cause a bad de-link.... ! Can we try without this? What about concurrent de-linking, yeah 
		// that stuff about versioning!.. And insertions need to *PRESERVE*, not increase version.

		// Retry if an inserter just linked front node to an upper layer..
		if ((expectedFront[atLayer].operator node_type * () == frontNode) &&
			cas_to_head(expectedFront[atLayer], desiredFront[atLayer], atLayer)) {
#if defined GDUL_CPQ_DEBUG
			if (desiredFront[atLayer].get_version() == 5) {
				performStore = true;
			}
#endif
			continue;
		}
	}

	// De-link base layer
	const bool result(cas_to_head(expectedFront[0], desiredFront[0], 0));
#if defined GDUL_CPQ_DEBUG
	if (performStore) {
		std::copy(std::begin(expectedFront), std::end(expectedFront), t_nonsense.actual);
		std::copy(std::begin(desiredFront), std::end(desiredFront), t_nonsense.next);
		std::copy(std::begin(expectedCopy), std::end(expectedCopy), t_nonsense.expected);
		std::copy(std::begin(t_recentOp.result), std::end(t_recentOp.result), t_nonsense.result);
		t_nonsense.opNode = frontNode;
		t_nonsense.m_sequenceIndex = m_sequenceCounter.load(std::memory_order_relaxed);
		t_nonsense.m_threadRole = t_threadRole;
	}
#endif
	return result;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::clear()
{
	// Perhaps just call this unsafe_clear..

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[cpq_detail::Max_Node_Height - i - 1].store(&m_head);
	}

#if defined (GDUL_CPQ_DEBUG)
	t_recentPops.clear();
	t_recentPushes.clear();
#endif
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::empty() const noexcept
{
	return at_end(m_head.m_next[0].load(std::memory_order_relaxed));
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	node_type* const baseLayerCurrent(atSet[0]);

	node_view& expected(expectedSet[0]);
	node_view desired(node);

	if (at_head(baseLayerCurrent)) {
		desired.set_version(expected.get_version() + 1);
	}

#if defined GDUL_CPQ_DEBUG
	node_view_set expectedCopy;
	std::copy(std::begin(expectedSet), std::end(expectedSet), expectedCopy);
#endif

	const bool result(baseLayerCurrent->m_next[0].compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed));
	if (!result) {
		return false;
	}
#if defined GDUL_CPQ_DEBUG
	node->m_inserted = 1;
	node->m_linkVersion = expected.get_version();
	baseLayerCurrent->m_nextView[0] = node;
	t_recentOp.opNode = node;
	t_recentOp.result[0] = result;
	t_recentOp.result[1] = false;
	t_recentOp.frontSwap = false;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
#endif

	try_link_upper(atSet, expectedSet, node);

#if defined GDUL_CPQ_DEBUG
	std::copy(std::begin(expectedSet), std::end(expectedSet), t_recentOp.actual);
	std::copy(std::begin(expectedCopy), std::end(expectedCopy), t_recentOp.expected);
#endif

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_upper(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		node_view& expected(expectedSet[layer]);
		node_view desired(node);
		node_type* const current(atSet[layer]);

		if (at_head(current)) {
			desired.set_version(expected.get_version() + 1);
		}

		const bool result(current->m_next[layer].compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed));
#if defined GDUL_CPQ_DEBUG
		t_recentOp.result[layer] = result;
		current->m_nextView[layer] = node;
#endif
		if (!result) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_splice_to_head(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& frontSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& replacementSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
#if defined GDUL_CPQ_DEBUG
	node_view_set expectedCopy;
	std::copy(std::begin(frontSet), std::end(frontSet), expectedCopy);
#endif
	// Mimick version of what should be at front node
	const node_view desired(node, replacementSet[0].get_version());
	replacementSet[0] = desired;

	if (try_swap_front(frontSet, replacementSet)) {

#if defined GDUL_CPQ_DEBUG
		t_recentOp.frontSwap = true;
		t_recentOp.m_sequenceIndex = m_sequenceCounter++;
#endif
		node_view_set headSet;
		std::uninitialized_fill(std::begin(headSet), std::end(headSet), node_view(&m_head));

		try_link_upper(headSet, replacementSet, desired);

#if defined GDUL_CPQ_DEBUG
		std::copy(std::begin(frontSet), std::end(frontSet), t_recentOp.actual);
		std::copy(std::begin(expectedCopy), std::end(expectedCopy), t_recentOp.expected);
		std::copy(std::begin(replacementSet), std::end(replacementSet), t_recentOp.next);
		t_recentOp.opNode = node;
#endif

		return true;
	}

	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::cas_to_head(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& desired, std::uint8_t layer)
{
	desired.set_version(expected.get_version() + 1);

	const bool result(m_head.m_next[layer].compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed));
#if defined GDUL_CPQ_DEBUG
	t_recentOp.result[layer] = result;
#endif
	return result;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::set_version(std::uint32_t version, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& to, std::uint8_t usingHeight)
{
	auto begin(std::begin(to));
	auto end(std::begin(to));
	end += usingHeight;

	std::for_each(begin, end, [version](node_view& n) {n.set_version(version); });
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::load_set(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, std::uint8_t offset)
{
	const std::uint8_t height(at->m_height);
	for (std::uint8_t i = offset; i < height; ++i) {
		outSet[i] = at->m_next[i].load(std::memory_order_seq_cst);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	node_view_set currentSet{};
	node_view_set nextSet{};

	prepare_insertion_sets(currentSet, nextSet, node);

	for (std::uint8_t i = 0, height = node->m_height; i < height; ++i) {
		node->m_next[i].store(node_view(nextSet[i].operator node_type * (), 0), std::memory_order_relaxed);
	}

	// Inserting at head
	if (at_head(currentSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 1;
#endif
		return try_link(currentSet, nextSet, node);
	}

	// Inserting after unflagged node atleast beyond head
	if (!nextSet[0].get_version()) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 2;
#endif
		return try_link(currentSet, nextSet, node);
	}

	// optimize this... Can probably use currentSet & nextSet, but for correctness, reload totally.
	node_view_set frontSet;
	load_set(frontSet, &m_head);

	// Inserting after front node...
	if (currentSet[0] == frontSet[0]) {

		// optimize this... Can probably use currentSet & nextSet, but for correctness, reload totally.
		node_view_set replacementSet;
		load_set(replacementSet, frontSet[0]);

		// Front node is in the middle of deletion, attempt special case splice to head
		if (replacementSet[0].get_version() == frontSet[0].get_version() + 1) {

			for (std::uint8_t i = 0, height = node->m_height; i < height; ++i) {
				node->m_next[i].store(replacementSet[i], std::memory_order_relaxed);
			}
#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 3;
#endif
			return try_splice_to_head(frontSet, replacementSet, node);
		}

		// Front node has garbage version, having been supplanted previously in the middle of deletion
		else {
#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 4;
#endif
			return try_link(currentSet, nextSet, node);
		}
	}

	// Inserting after flagged node that has been supplanted in the middle of deletion
	if (find_node(currentSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 5;
#endif
		return try_link(currentSet, nextSet, node);
	}

	// Cannot insert after a node that is not in list
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

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::at_head(const concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* n) const
{
	return at_end(n);
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
		for (std::size_t i = rd() % 1000; i != 0; --i) {
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
	result = 2;
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











// Here's current issue:

// HEAD------------------1--------------------END
// HEAD------------------1--------------------END

// inserter loads HEAD layer 1, stalls
// deleter deletes FRONT
// :
// HEAD------------------END
// HEAD------------------END
// 
// inserter loads HEAD layer 0, inserts 0.5:
// HEAD-----------------0.5------------------*1*------------------END
// HEAD-----------------0.5-------------------END

// -I feel like I've been over this scenario! ? Perhaps I intended for height to decay?

// This, of course may only be an issue at HEAD node, since deleters may do not touch any other node.
// So, we don't want to use a de-linked node in the upper layers of our new insert. 
// If we reload upper layers when preparing head insertion set, what then? That'll mean the upper nodes in 
// insertion set will be guaranteed to be equal to or younger than bottom node. Which meeeans! ? In case bottom 
// linkage succeeds upper may still fail? If bottom fails, bottom fails. The nodes referenced in the inserted node will
// be guaranteed to be younger than the bottom node, and in case insertion succeeds, this means we will never refer to stale
// nodes!.. !






// Further notes:
// Investigate total guarantee of age of reference when linking. Might still be some cases where stale nodes may be referenced.. !

// HEAD------------------1-----------------------------------------END
// HEAD------------------1--------------------2--------------------END


// Loading 1 layer 1, is there anywhere we may link if it is removed? 

// No, links may only de-link at HEAD, and we have handled that case. 

// What about replacementSet ? Does that need a different load order? And nextSet for that matter?

// Both are currently loaded up->down and may essentially be the new set at HEAD...... ?









// What about splicing... What are we doing then? We're doing a de-link with our own node inserted. What then? Can we guarantee 
// validity of HEAD->FRONT->[next] pointers.. Well, they should be valid. There must be no living links to FRONT when bottom layer 
// is de linked. 

// What about head then. Upper layers... What are the different situations that can happen here? Is there a way for a stale reference
// to sneak in via failed de link? We can't expect any front node to be fully linked. The front node may be currently linking, and 
// a deleter begins to remove it. Prevention of further linkage must be guaranteed. Version should prevent this fine. We're always
// increasing version by cas in the upper layers before a de link, this should prevent further linkage. Can one of these cas' fail ? I suppose
// we could just ensure that the version was one we want to see(not-below or equal. Below being defined as [current - 256] or something)





// HEAD------------------END
// HEAD------------------END
// Inserter links 1 ->
// HEAD------------------1------------------END
// HEAD------------------1------------------END

// From here, inserter prepares an insertion set 
// [HEAD][->1]
// [HEAD][->1]
// and succeeds linking at both layers to node(0)

// Deleter removes 0 first & 1 second.



// empty -> 1 -> 0, 1 -> 1 -> empty, with 0 link at upper...

// So the inserter first links 1, then 0. This could make more sense, since 0 was the stray
// link found in upper layer.

// So somehow inserter links upper layer to 0 after deleter removes 1 & has loaded whatever was in that place

// When the deleter removed 1, it expected HEAD links to be:
// [->?]
// [->1] 
// but found 
// [->0]
// [->1]

// When inserter linked upper layer to 0 it expected [->1][->1]. This is could be the case:

// Inserter loads [->1][->1 (version 0 )], links base layer to 0
// Deleter loads head set, links both layers to 1, with upper being version 0
// Deleter loads [->1][->1]
// Inserter links upper layer to 0, since version is still 0
// Deleter attempts to de link 1 at upper but finds 0!

// In short. Versioning at HEAD needs to be sorted. Probably, all head links should increase independantly



//So deleter expected to find 1, version 1 in upper slot.Let's see.. This version should only exist after first insert. So 
//we failed upper link when deleting 0 ? That doens't make sense. Inserter linked 0 with expected version 1 at both slots. All in
//order..
//
//Here deleter should find 0, 0 with versions 2, 2. If it loads HEAD set partially, it may find 0, 1 with versions 2, 1
//So in this case:
//	Deleter loads HEAD, finds 0, 1 with version 2, 1
//		Deleter attempts delink at upper, fails, delinks lower.->HEAD contains 1, 1 with versions 3, 1
//		Deleter loads HEAD finds 1, 1, versions 3, 1
//		Inserter links upper to 0->HEAD contains 1, 0 versions 3, 2
//		Deleter attempts delink at upper, fails
//		Deleter delink lower.
//
//		So how did the delink fail at upper with 0, 1 ?
//


// So we have a fundamental issue with our loading order in try_pop. Since we are loading upper layers first, these may out-age base layer. This will cause failures in de-linking, 
// followed by a delink at base layer. 

// What can we do to fix this? We may enforce delinkage at upper layers by binding it to total op failure. We can also try reversing load order. But if I recall correctly, this had
// other issues.

// Hmm. Inverting load order actually worked well. Very well. I need to work out why.

// So if age of upper layers may be younger than that of bottom. In case we see a more recent node at layer 1, we might end up delinking at layer 1.. Could we cause inversion of forward pointers?


// If we have 1->2->3 at upper, and expect 2 at bottom, we could want to link 3 where 1 resided. That's fine. Well. Ok then.. I'm sure problems'll show themselves when we increase layers and/or list length..

// Deleter removes 0(2)1(1)0(3)
// Inserter links 1, 0, 0



// Want retry buffer! 


//So if two deleters compete: 
//
//	* both load front set from bottom
//	* first loads bottom layer
//	* other loads bottom layer
//	* first delinks upper layers
//	* other loads upper layers
//	* first delinks bottom
//	* other 'delinks' upper layers (now referring to stale node!)

// This is why we need to make versioning more uniform with regards to bottom layer. Everything should slave to bottom layer, THAT is how we keep things in order.

// In this case, we could make the assumption that if the upper layers were 'higher' then that of base layer version, they could not be a part
// of the currently inspected front node link.
// Will have to figure out how to handle HEAD insertions. Should those increase version? It is a fact that the values there should be assumed to be 'new'.
// That is, they shouldn't be subject to ABA issues. 
// This approach is more general & clear than the current, and has the added benefit of making the 'second case' upper layer HEAD linking (where we
// check for recent linkage of current node) obsolete. -- We get that for free (along with any other similar situations).