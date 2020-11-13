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
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

//#define GDUL_CPQ_DEBUG
//#define DYNAMIC_SCAN

// Todo must move version handling to functions so that edge cases around max_version is handled properly.

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
static constexpr std::uint32_t Max_Version_Mask = (std::numeric_limits<std::uint32_t>::max() >> (16 - 3));
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

constexpr std::uint32_t version_delta(std::uint32_t from, std::uint32_t to)
{
	const std::uint32_t delta(to - from);
	return delta & Max_Version_Mask;
}
constexpr std::uint32_t version_step_one(std::uint32_t from)
{
	const std::uint32_t next(from + 1);
	const std::uint32_t zeroAdjust(!(bool)next);
	const std::uint32_t zeroAvoid(next + zeroAdjust);
	return zeroAvoid & Max_Version_Mask;
}
constexpr std::uint32_t version_step(std::uint32_t from, std::uint8_t step)
{
	const std::uint32_t result(version_step_one(from));
	if (step == 1) {
		return result;
	}
	return version_step_one(result);
}

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
		std::uint32_t m_flaggedVersion = 0;
		std::uint32_t m_preFlagVersion = 0;
		std::uint8_t m_delinked = 0;
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

	bool try_delink_front(node_view_set& expectedFront, node_view_set& desiredFront, std::uint8_t versionOffset = 1);
	bool try_replace_front(node_view_set& current, node_view_set& next, node_view node);

	bool try_link_to_head(node_view_set& next, node_view node);
	void try_link_to_head_upper(node_view_set& next, node_view node, std::uint32_t version);

	bool check_for_delink_helping(node_view of, node_view actual, node_view triedReplacement);

	// Preliminarily perform when desired base layer version increases the Max_Entries_Hint multiple (expectedV / Max_Entries_Hint) < (desiredV / Max_Entries_Hint) 
	// This should be cheap and may be performed by both inserters and deleters.
	void perform_version_upkeep(node_view_set& at, std::uint32_t baseLayerVersion, std::uint8_t versionStep);

	static bool in_range(std::uint32_t version, std::uint32_t inRangeOf);
	static void load_set(node_view_set& outSet, node_type* at, std::uint8_t offset = 0);
	static void try_link_to_node_upper(node_view_set& at, node_view_set& next, node_view node);
	static bool try_link_to_node(node_view_set& at, node_view_set& next, node_view node);

	static cpq_detail::flag_node_result flag_node(node_view at, node_view& next);

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
		std::thread::id m_threadId;
	};
	inline static thread_local recent_op_info t_recentOp = recent_op_info();
	inline static std::atomic_uint s_recentPushIx = 0;
	inline static std::atomic_uint s_recentPopIx = 0;
	inline static gdul::shared_ptr<recent_op_info[]> s_recentPushes = gdul::make_shared<recent_op_info[]>(16);
	inline static gdul::shared_ptr<recent_op_info[]> s_recentPops = gdul::make_shared<recent_op_info[]>(16);

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
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif
	while (!try_push(node));

#if defined (GDUL_CPQ_DEBUG)
	t_recentOp.opNode = node;
	node->m_inserted = 1;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPushes[s_recentPushIx++] = t_recentOp;
#endif
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
{
#if defined GDUL_CPQ_DEBUG
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif

	cpq_detail::flag_node_result flagResult(cpq_detail::flag_node_unexpected);
	bool deLinked(false);

	node_type* nodeClaim(nullptr);

	do {
		node_view_set frontSet{};
		node_view_set nextSet{};

		node_view nodeClaimView(m_head.m_next[0].load(std::memory_order_relaxed));
		nodeClaim = nodeClaimView;

		if (at_end(nodeClaim)) {
			return false;
		}

		load_set(nextSet, nodeClaim);
#if defined GDUL_CPQ_DEBUG
		const std::uint32_t preVersion(nextSet[0].get_version());
#endif

		// Right so this seems to work.
		// I just realized though that this is part of a larger issue. Having old flagged nodes sitting around in the list can cause other issues. 
		// I should probably see about finding a way to reset the flagging. 
		// ---------------------------- !--------------------------------------------------
		//const node_view reload = m_head.m_next[0].load(std::memory_order_relaxed);
		//if (nodeClaimView != reload) {
		//	continue;
		//}
		//nodeClaimView = reload;
		// ---------------------------- !--------------------------------------------------

		std::fill(std::begin(frontSet), std::begin(frontSet) + nodeClaim->m_height, nodeClaimView);

		flagResult = flag_node(frontSet[0], nextSet[0]);

#if defined GDUL_CPQ_DEBUG
		if (flagResult == cpq_detail::flag_node_success) {
			nodeClaim->m_flaggedVersion = nextSet[0].get_version();
			nodeClaim->m_preFlagVersion = preVersion;
		}
#endif
		if (flagResult == cpq_detail::flag_node_unexpected) {
			continue;
		}

		deLinked = try_delink_front(frontSet, nextSet);

		if (!deLinked && flagResult == cpq_detail::flag_node_success) {

			deLinked = check_for_delink_helping(nodeClaimView, frontSet[0], nextSet[0]);
		}

#if defined GDUL_CPQ_DEBUG
		else if (deLinked) {
			nodeClaim->m_delinked = 1;
		}
#endif

	} while (flagResult != cpq_detail::flag_node_success || !deLinked);

	out = nodeClaim;

#if defined GDUL_CPQ_DEBUG
	nodeClaim->m_removed = 1;
	t_recentOp.opNode = nodeClaim;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPops[s_recentPopIx++] = t_recentOp;
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

	if (at_head(atSet[0])) {
		load_set(nextSet, &m_head, 1);
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::flag_node_result concurrent_priority_queue<Key, Value, Compare, Allocator>::flag_node(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view at, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& next)
{
	// if we see a 'higher' version expected at next than that at head
	// we should simply return flag_node_unexpected...

	// The problem is we might have a node circulating with v1 for Max_Version times. Then that will appear
	// to be of higher version. Forever. And we'll have a lockup.

	// We can, of course, reload head to see if version has increased there. 



	const std::uint32_t nextVersion(cpq_detail::version_step_one(at.get_version()));

	if (next.get_version() == nextVersion) {
		return cpq_detail::flag_node_compeditor;
	}

	if (at.operator node_type * ()->m_next[0].compare_exchange_strong(next, node_view(next, nextVersion), std::memory_order_relaxed)) {
		next.set_version(nextVersion);
		return cpq_detail::flag_node_success;
	}

	if (next.get_version() == nextVersion) {
		return cpq_detail::flag_node_compeditor;
	}

	return cpq_detail::flag_node_unexpected;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_delink_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& expectedFront, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& desiredFront, std::uint8_t versionOffset)
{
	const std::uint8_t frontHeight(expectedFront[0].operator const node_type * ()->m_height);
	const std::uint8_t numUpperLayers(frontHeight - 1);
	const std::uint32_t baseLayerVersion(expectedFront[0].get_version());
	const std::uint32_t upperLayerNextVersion(cpq_detail::version_step_one(baseLayerVersion));


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

	const std::uint32_t bottomLayerNextVersion(cpq_detail::version_step(baseLayerVersion, versionOffset));

	perform_version_upkeep(expectedFront, baseLayerVersion, versionOffset);

	return exchange_node_link(&m_head, expectedFront[0], desiredFront[0], bottomLayerNextVersion, 0) == cpq_detail::exchange_link_success;
}

template<class Key, class Value, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, Compare, Allocator>::clear()
{
	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		m_head.m_next[cpq_detail::Max_Node_Height - i - 1].store(&m_head);
	}

#if defined (GDUL_CPQ_DEBUG)
	s_recentPushIx = 0;
	s_recentPopIx = 0;
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

	return true;
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_link_to_head(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	const std::uint32_t baseLayerVersion(next[0].get_version());
	const std::uint32_t baseLayerNextVersion(cpq_detail::version_step_one(baseLayerVersion));

	perform_version_upkeep(next, baseLayerVersion, 1);

	if (exchange_node_link(&m_head, next[0], node, baseLayerNextVersion, 0) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_head_upper(next, node, baseLayerNextVersion);

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
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_replace_front(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& frontSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& nextSet, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view node)
{
	nextSet[0] = node;

	if (try_delink_front(frontSet, nextSet, 1)) {

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
	if (atSet[0] == front) {
		// In case we are looking at front node, we need to make sure our view of head base layer version 
		// is younger than that of front base layer version. Else we might think it'd be okay with a
		// regular link at front node in the middle of deletion!
		atSet[0] = front;

		// Front node is in the middle of deletion, attempt special case splice to head
		if (nextSet[0].get_version() == cpq_detail::version_step_one(atSet[0].get_version())) {

#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 3;
#endif
			return try_replace_front(atSet, nextSet, node);
		}
	}

	// Inserting after flagged node that has been supplanted in the middle of deletion
	if (find_node(atSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 4;
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
	const std::uint32_t nextVersion(cpq_detail::version_step(baseLayerVersion, versionStep));
	const std::uint32_t maxEntriesMultiPost(nextVersion / cpq_detail::Max_Entries_Hint);
	const std::uint8_t delta(maxEntriesMultiPost - maxEntriesMultiPre);

	if (delta) {
		// DO stuff..
	}
}
template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::check_for_delink_helping(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view of, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view actual, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view triedReplacement)
{
	const std::uint32_t triedVersion(triedReplacement.get_version());
	const std::uint32_t actualVersion(actual.get_version());
	const bool matchingVersion(triedVersion == actualVersion);

	// All good, some other actor did our replacment for us
	if (actual == triedReplacement &&
		matchingVersion) {
		return true;
	}

	// If we can't find the node in the structure and version still belongs to us, it must be ours :)
	if (!find_node(of)) {
		const node_type* const ofNode(of);
		const std::uint32_t currentVersion(ofNode->m_next[0].load(std::memory_order_relaxed).get_version());
		const bool stillUnclaimed(currentVersion == triedVersion);

		return stillUnclaimed;
	}

	return false;
}
template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::in_range(std::uint32_t version, std::uint32_t inRangeOf)
{
	return cpq_detail::version_delta(version, inRangeOf) < cpq_detail::In_Range_Delta;
}

template<class Key, class Value, class Compare, class Allocator>
inline cpq_detail::exchange_link_result concurrent_priority_queue<Key, Value, Compare, Allocator>::exchange_head_link(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type* at, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view& desired, std::uint32_t desiredVersion, std::uint8_t layer)
{
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

		if (!in_range(expectedVersion, desiredVersion)) {
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

	return result;
}
}
}

#pragma warning(pop)