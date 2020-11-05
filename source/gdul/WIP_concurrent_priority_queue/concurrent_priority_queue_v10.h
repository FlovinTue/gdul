#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <thread>
#include <cassert>
#include <utility>
#include <random>
#include <iostream>
#include "../Testers/Common/util.h"

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

#define GDUL_CPQ_DEBUG

namespace gdul {
namespace cpq_detail {

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

static constexpr std::uintptr_t Bottom_Bits = (((1 << 1) | 1) << 1) | 1;
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Bottom_Bits;
static constexpr std::uintptr_t Version_Mask = ~Pointer_Mask;
static constexpr std::uint32_t Max_Version_Mask = (std::numeric_limits<std::uint16_t>::max() * 2 * 2 * 2) - 1;
static constexpr std::uint8_t Cache_Line_Size = 64;
static constexpr std::uint8_t In_Range_Delta = std::numeric_limits<std::uint8_t>::max();
static constexpr size_type Max_Entries_Hint = 1024;
static constexpr std::uint8_t Max_Node_Height = 2;// cpq_detail::calc_max_height(Max_Entries_Hint);

enum exchange_link_result : std::uint8_t
{
	exchange_link_null_value = 0,
	exchange_link_outside_range = 1,
	exchange_link_success = 2,
	exchange_link_other_link = 3, 
};
enum flag_node_result : std::uint8_t
{
	flag_node_null = 0,
	flag_node_unexpected = 1,
	flag_node_compeditor = 2,
	flag_node_success = 3,
};

static std::uint8_t random_height();


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
		std::atomic<node_view> m_next[Max_Node_Height]{};
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
	void prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node);

	bool at_end(const node_type*) const;
	bool at_head(const node_type*) const;


	bool try_push(node_type* node);

	bool try_delink_front(node_view_set& expectedFront, node_view_set& desiredFront, std::uint8_t versionOffset = 0);
	bool try_replace_front(node_view_set& current, node_view_set& next, node_view node);

	bool try_link_to_head(node_view_set& next, node_view node);
	void try_link_to_head_upper(node_view_set& next, node_view node, std::uint32_t version);

	// Preliminarily perform when desired base layer version increases the Max_Entries_Hint multiple (expectedV / Max_Entries_Hint) < (desiredV / Max_Entries_Hint) 
	// This should be cheap and may be performed by both inserters and deleters.
	void perform_version_upkeep(node_view_set& at, std::uint32_t baseLayerVersion, std::uint8_t versionStep);

	static bool in_range(std::uint32_t version, std::uint32_t inRangeOf);
	static void load_set(node_view_set& outSet, node_type* at, std::uint8_t offset = 0);
	static void try_link_to_node_upper(node_view_set& at, node_view_set& next, node_view node);
	static bool try_link_to_node(node_view_set& at, node_view_set& next, node_view node);

	static cpq_detail::flag_node_result flag_node(node_view node, node_view next);
	
	static cpq_detail::exchange_link_result exchange_head_link(node_type* at, node_view& expected, node_view& desired, std::uint32_t desiredVersion, std::uint8_t layer);
	static cpq_detail::exchange_link_result exchange_node_link(node_type* at, node_view& expected, node_view& desired, std::uint32_t desiredVersion, std::uint8_t layer);


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
		expanded_node_view actual[cpq_detail::Max_Node_Height];
		expanded_node_view desired[cpq_detail::Max_Node_Height];

		const node_type* opNode = nullptr;

		cpq_detail::exchange_link_result result[cpq_detail::Max_Node_Height];

		uint32_t m_sequenceIndex = 0;
		uint8_t m_insertionCase = 0;
		std::uint8_t m_threadRole = 0;
	};
	inline static thread_local recent_op_info t_recentOp = recent_op_info();
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

	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif
	while (!try_push(node));

#if defined (GDUL_CPQ_DEBUG)
	t_recentOp.opNode = node;
	node->m_inserted = 1;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentPushes.push_back(t_recentOp);
#endif
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
#if defined GDUL_CPQ_DEBUG
	t_threadRole = 1;
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif

	cpq_detail::flag_node_result flagResult(cpq_detail::flag_node_unexpected);
	bool deLinked(false);

	node_type* frontNode(nullptr);

	do {
		node_view_set frontSet{};
		node_view_set replacementSet{};

		//load_set(frontSet, &m_head);

		frontSet[0] = m_head.m_next[0].load();

		frontNode = frontSet[0];

		std::fill(std::begin(frontSet) + 1, std::end(frontSet), frontSet[0]);

		if (at_end(frontNode)) {
			return false;
		}

		load_set(replacementSet, frontNode);

		flagResult = flag_node(frontSet[0], replacementSet[0]);

		if (flagResult == cpq_detail::flag_node_unexpected) {
			continue;
		}

		deLinked = try_delink_front(frontSet, replacementSet);

		if (!deLinked && flagResult == cpq_detail::flag_node_success) {

			deLinked = (frontSet[0] == replacementSet[0]) || !find_node(frontNode);
		}

	} while (flagResult != cpq_detail::flag_node_success || !deLinked);

	out = frontNode;

#if defined GDUL_CPQ_DEBUG
	frontNode->m_removed = 1;
	//frontNode->m_delinkVersion = frontSet[0].get_version();
	t_recentOp.opNode = frontNode;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentPops.push_back(t_recentOp);
#endif

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node)
{
	atSet[cpq_detail::Max_Node_Height - 1] = &m_head;

	const key_type key(node->m_kv.first);

	for (std::uint8_t i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const std::uint8_t layer(cpq_detail::Max_Node_Height - i - 1);

		for (;;) {
			node_type* const currentNode(atSet[layer]);
			nextSet[layer] = currentNode->m_next[layer].load(std::memory_order_seq_cst);
			node_type* const nextNode(nextSet[layer]);

			if (!m_comparator(nextNode->m_kv.first, key)) {
				break;
			}

			atSet[layer] = nextNode;
		}

		if (layer) {
			atSet[layer - 1] = atSet[layer];
		}
	}

	// Why does this work? Age and all that? But what does that matter? (it's only upper layers)
	if (at_head(atSet[0])) {
		std::fill(std::begin(nextSet) + 1, std::begin(nextSet) + node->m_height, nextSet[0]);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::flag_node_result concurrent_priority_queue<Key, Value, Compare, Allocator>::flag_node(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view next)
{
	// Small chance of loop around and deadlocking

	const std::uint32_t nextVersion(node.get_version() + 1);

	if (next.get_version() == nextVersion) {
		return cpq_detail::flag_node_compeditor;
	}

	if (node.operator node_type* ()->m_next[0].compare_exchange_strong(next, node_view(next, nextVersion), std::memory_order_relaxed)) {
		return cpq_detail::flag_node_success;
	}

	if  (next.get_version() == nextVersion) {
		return cpq_detail::flag_node_compeditor;
	}

	return cpq_detail::flag_node_unexpected;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_delink_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedFront, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& desiredFront, std::uint8_t versionOffset)
{
	const std::uint8_t frontHeight(expectedFront[0].operator const node_type * ()->m_height);
	const std::uint8_t numUpperLayers(frontHeight - 1);
	const std::uint32_t upperLayerNextVersion(expectedFront[0].get_version() + 1);


	for (std::uint8_t i = 0; i < numUpperLayers; ++i) {
		const std::uint8_t layer(frontHeight - 1 - i);

		const cpq_detail::exchange_link_result result(exchange_head_link(&m_head, expectedFront[layer], desiredFront[layer], upperLayerNextVersion, layer));

		if (result == cpq_detail::exchange_link_outside_range) {
			return false;
		}
		if (result == cpq_detail::exchange_link_success) {
			expectedFront[layer] = desiredFront[layer];
		}
	}

	const std::uint32_t bottomLayerNextVersion(upperLayerNextVersion + versionOffset);

	perform_version_upkeep(expectedFront, bottomLayerNextVersion);

	return exchange_node_link(&m_head, expectedFront[0], desiredFront[0], bottomLayerNextVersion, 0) == cpq_detail::exchange_link_success;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::clear()
{
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
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_to_node(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	node_type* const baseLayerNode(atSet[0]);

	if (exchange_node_link(baseLayerNode, expectedSet[0], node, 0, 0) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_node_upper(atSet, expectedSet, node);

#if defined GDUL_CPQ_DEBUG
	node.operator node_type*()->m_linkVersion = 0;
#endif
	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_to_head(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	std::uint32_t nextVersion(next[0].get_version() + 1);

	perform_version_upkeep(next, nextVersion);

	if (exchange_node_link(&m_head, next[0], node, nextVersion, 0) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_head_upper(next, node, nextVersion);

#if defined GDUL_CPQ_DEBUG
	node.operator node_type* ()->m_linkVersion = nextVersion;
#endif

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_to_head_upper(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node, std::uint32_t version)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {

		if (exchange_head_link(&m_head, expectedSet[layer], node, version, layer) == cpq_detail::exchange_link_outside_range) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_to_node_upper(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {

		exchange_node_link(atSet[layer], expectedSet[layer], node, expectedSet[layer].get_version(), layer);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_replace_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& frontSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& replacementSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	replacementSet[0] = node;

	if (try_delink_front(frontSet, replacementSet, 1)) {

		try_link_to_head_upper(frontSet, node, frontSet[0].get_version());

		return true;
	}

	return false;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::load_set(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, std::uint8_t offset)
{
	const std::uint8_t atHeight(at->m_height);
	for (std::uint8_t i = offset; i < atHeight; ++i) {
		outSet[i] = at->m_next[i].load(std::memory_order_seq_cst);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_push(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* node)
{
	node_view_set atSet{};
	node_view_set nextSet{};

	prepare_insertion_sets(atSet, nextSet, node);

	for (std::uint8_t i = 0, height = node->m_height; i < height; ++i) {
		node->m_next[i].store(node_view(nextSet[i].operator node_type * (), 0), std::memory_order_relaxed);
	}

	// Inserting at head
	if (at_head(atSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 1;
#endif
		return try_link_to_head(nextSet, node);
	}

	// Inserting after unflagged node atleast beyond head
	if (!nextSet[0].get_version()) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 2;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	const node_view front(m_head.m_next[0].load(std::memory_order_seq_cst));

	// Inserting after front node...
	if (atSet[0] == front){
		// In case we are looking at front node, we need to make sure our view of head base layer version 
		// is younger than that of front base layer version. Else we might think it'd be okay with a
		// regular link at front node in the middle of deletion!
		atSet[0] = front;

		// Front node is in the middle of deletion, attempt special case splice to head
		if (nextSet[0].get_version() == atSet[0].get_version() + 1) {
			
#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 3;
#endif
			return try_replace_front(atSet, nextSet, node);
		}

		// All good, nothing going on with front node. Just insert regularly
		else {
#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 4;
#endif
			return try_link_to_node(atSet, nextSet, node);
		}
	}

	// Inserting after flagged node that has been supplanted in the middle of deletion
	if (find_node(atSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 5;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	// Cannot insert after a node that is not in list
	return false;
}
template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::perform_version_upkeep(node_view_set& at, std::uint32_t baseLayerVersion, std::uint8_t versionStep)
{
	const std::uint32_t maxEntriesMultiPre(baseLayerVersion / cpq_detail::Max_Entries_Hint);
	const std::uint32_t nextVersion(baseLayerVersion + versionStep);
	const std::uint32_t maxEntriesMultiPost(nextVersion / cpq_detail::Max_Entries_Hint);
	const std::uint8_t delta(maxEntriesMultiPost - maxEntriesMultiPre);

	if (delta) {
		// DO stuff..
	}
}
template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::in_range(std::uint32_t version, std::uint32_t inRangeOf)
{
	const std::uint32_t delta(version - inRangeOf);
	const std::uint32_t deltaActual(delta & cpq_detail::Max_Version_Mask);
	return deltaActual < cpq_detail::In_Range_Delta;
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::exchange_link_result concurrent_priority_queue<Key, Value, Compare, Allocator>::exchange_head_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& desired, std::uint32_t desiredVersion, std::uint8_t layer)
{
	// In case version is !in_range(), that means base layer has recieved one or more modifications, and a new one is in progress. Abort.
	// In case the version is in_range() try again. Layer could be a couple versions lower, having just been increased (but still in_range()) by a slow inserter.
	// In case version is equal, it may be that another node was inserted or that other deleters delinked. We can't know from here. Return true to indicate possible success.

	// Note on last case (equal) it could be that we may not need to consider the errant case of inserter linking to an upper layer (but not bottom). This means we could simply
	// check for equality of value to the desired node_view

#if defined GDUL_CPQ_DEBUG
	t_recentOp.expected[layer] = expected;
#endif

	desired.set_version(desiredVersion);

	cpq_detail::exchange_link_result result(cpq_detail::exchange_link_success);

	do {
		const std::uint32_t expectedVersion(expected.get_version());
		
		if (expectedVersion == desiredVersion) {
			result = cpq_detail::exchange_link_other_link;
			break;
		}

		if (!in_range(desiredVersion, expectedVersion)) {
			result = cpq_detail::exchange_link_outside_range;
			break;
		}

	} while (!at->m_next[layer].compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed));


#if defined GDUL_CPQ_DEBUG
	t_recentOp.result[layer] = result;
	t_recentOp.actual[layer] = expected;
	t_recentOp.desired[layer] = desired;
	if (result && expected.get_version()) {
		at->m_nextView[layer] = desired;
	}
#endif

	return result;
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::exchange_link_result concurrent_priority_queue<Key, Value, Compare, Allocator>::exchange_node_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& desired, std::uint32_t desiredVersion, std::uint8_t layer)
{
#if defined GDUL_CPQ_DEBUG
	t_recentOp.expected[layer] = expected;
#endif

	desired.set_version(desiredVersion);

	cpq_detail::exchange_link_result result(cpq_detail::exchange_link_outside_range);

	if (at->m_next[layer].compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
		result = cpq_detail::exchange_link_success;
	}

#if defined GDUL_CPQ_DEBUG
	t_recentOp.result[layer] = result;
	t_recentOp.actual[layer] = expected;
	t_recentOp.desired[layer] = desired;
	if (result && expected.get_version()) {
		at->m_nextView[layer] = desired;
	}
#endif

	return result;
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