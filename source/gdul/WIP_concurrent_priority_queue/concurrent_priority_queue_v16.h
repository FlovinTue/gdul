#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <gdul/memory/concurrent_scratch_pool.h>
#include <gdul/memory/concurrent_guard_pool.h>

#define GDUL_CPQ_GUARD_POOL
//#define GDUL_CPQ_DEBUG
// ---- to delete -------------
#if defined GDUL_CPQ_DEBUG
#include <cassert>
#include <utility>
#include <thread>
#include "../Testers/Common/util.h"
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#endif

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

#pragma region detail

namespace gdul {
namespace cpq_detail {

using size_type = std::size_t;

static constexpr size_type log2_ceil(size_type value)
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
static constexpr std::uint8_t to_tower_height(size_type expectedListSize)
{
	const std::uint8_t naturalHeight((std::uint8_t)log2_ceil(expectedListSize));
	const std::uint8_t halfHeight(naturalHeight / 2 + bool(naturalHeight % 2));

	return halfHeight;
}
static constexpr size_type to_expected_list_size(std::uint8_t towerHeight)
{
	size_type base(1);

	for (std::uint8_t i = 0; i < towerHeight * 2; ++i) {
		base *= 2;
	}

	return base;
}

static constexpr std::uintptr_t Bottom_Bits = ~(std::uintptr_t(std::numeric_limits<std::uintptr_t>::max()) << 3);
static constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) & ~Bottom_Bits;
static constexpr std::uintptr_t Version_Mask = ~Pointer_Mask;
static constexpr std::uint32_t Max_Version = (std::numeric_limits<std::uint32_t>::max() >> (16 - 3));
static constexpr std::uint32_t In_Range_Delta = Max_Version / 2;

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

static std::uint32_t version_delta(std::uint32_t from, std::uint32_t to);
static std::uint32_t version_add_one(std::uint32_t from);
static std::uint32_t version_sub_one(std::uint32_t from);
static std::uint32_t version_step(std::uint32_t from, std::uint8_t step);
static std::uint8_t random_height(std::uint8_t);
static bool in_range(std::uint32_t version, std::uint32_t inRangeOf);

template <class Key, class Value, std::uint8_t LinkTowerHeight>
struct node;

template <class Pool>
struct pool_base;

struct allocation_strategy_identifier_pool;
struct allocation_strategy_identifier_scratch;
struct allocation_strategy_identifier_external;

template <class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
class concurrent_priority_queue_impl;

}
#pragma endregion

/// <summary>
/// Fast, without need for external application support
/// </summary>
/// <param name="Allocator">Internal allocator</param>
template <class Allocator>
struct cpq_allocation_strategy_pool;

/// <summary>
/// Faster, but needs periodic resetting of internal scratch pool
/// </summary>
/// <param name="Allocator">Internal allocator</param>
template <class Allocator>
struct cpq_allocation_strategy_scratch;

/// <summary>
/// Fastest (potentially), node allocation and reclamation is handled externally by the user
/// </summary>
struct cpq_allocation_strategy_external;

/// <summary>
/// A concurrency safe lock-free priority queue based on skip list design
/// </summary>
/// <param name="Key">Key type</param>
/// <param name="Value">Value type</param>
/// <param name="AllocationStrategy">Allocation and reclamation strategy for nodes</param>
/// <param name="ExpectedListSize">How many items is expected to be in the list at any one time</param>
/// <param name="Compare">Comparator to decide node ordering</param>
template <
	class Key,
	class Value,
	cpq_detail::size_type ExpectedListSize = 512,
	class AllocationStrategy = cpq_allocation_strategy_pool<std::allocator<std::uint8_t>>,
	class Compare = std::less<Key>
>
class concurrent_priority_queue : public cpq_detail::concurrent_priority_queue_impl<Key, Value, cpq_detail::to_tower_height(ExpectedListSize), AllocationStrategy, Compare>
{
public:
	using cpq_detail::concurrent_priority_queue_impl<Key, Value, cpq_detail::to_tower_height(ExpectedListSize), AllocationStrategy, Compare>::concurrent_priority_queue_impl;
};

namespace cpq_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
class concurrent_priority_queue_impl : public pool_base<typename AllocationStrategy::template pool_type<Key, Value, LinkTowerHeight>::type>
{
public:

	using key_type = Key;
	using value_type = Value;
	using compare_type = Compare;
	using node_type = node<Key, Value, LinkTowerHeight>;
	using size_type = typename size_type;
	using comparator_type = Compare;
	using allocation_strategy = AllocationStrategy;
	using allocator_type = typename allocation_strategy::allocator_type;
	using input_type = typename allocation_strategy::template input_type<Key, Value, LinkTowerHeight>::type;
	using allocation_strategy_identifier = typename allocation_strategy::identifier;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_priority_queue_impl();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="alloc">Allocator passed to node pool</param>
	concurrent_priority_queue_impl(allocator_type alloc);

	/// <summary>
	/// Destructor
	/// </summary>
	~concurrent_priority_queue_impl() noexcept;

	/// <summary>
	/// Enqueue an item
	/// </summary>
	/// <param name="in">Item to be enqueued</param>
	void push(const input_type& in);

	/// <summary>
	/// Enqueue an item
	/// </summary>
	/// <param name="in">Item to be enqueued using move semantics</param>
	void push(input_type&& in);

	/// <summary>
	/// Attempt to dequeue the top item
	/// </summary>
	/// <param name="out">Item to be written. Will be remain unmodified in case of failure</param>
	/// <returns>True on success</returns>
	bool try_pop(input_type& out);

	/// <summary>
	/// Remove any items from list
	/// </summary>
	void clear() noexcept;

	/// <summary>
	/// Query for items
	/// </summary>
	/// <returns>True if no items exists in list</returns>
	bool empty() const noexcept;

	/// <summary>
	/// Reset to initial state
	/// Assumes no concurrent inserts and an empty structure
	/// </summary>
	void unsafe_reset();

private:
	using node_view = typename node_type::node_view;
	using node_view_set = node_view[LinkTowerHeight];

	template <class In>
	void push_internal(In&& in);
	bool try_pop_internal(std::pair<Key, Value>& out);
	void clear_internal();

	bool try_push(node_type* node);
	bool prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node) const;

	bool at_end(const node_type* n) const;
	bool at_head(const node_type* n) const;
	bool is_flagged(node_view n) const;


	bool try_delink_front(node_view_set& expectedFront, node_view_set& desiredFront, std::uint8_t versionOffset, std::uint8_t frontHeight);
	bool try_replace_front(node_view_set& current, node_view_set& next, node_view node);

	bool try_link_to_head(node_view_set& next, node_view node);
	void try_link_to_head_upper(node_view_set& next, node_view node, std::uint32_t version);

	bool has_been_delinked_by_other(const node_type* of, node_view actual, node_view triedReplacement) const;

	bool exists_in_list(const node_type* node, const node_type* searchStart) const;

	void perform_version_upkeep(std::uint8_t aboveLayer, std::uint32_t baseVersion, std::uint32_t nextVersion, node_view_set& next);

	static void load_set(node_view_set& outSet, const node_type* at, std::uint8_t offset = 0, std::uint8_t max = LinkTowerHeight);
	static void unflag_node(node_type* at, node_view& next);
	static void link_to_node_upper(node_view_set& at, node_view_set& next, node_view node);

	static bool try_link_to_node(node_view_set& at, node_view_set& next, node_view node);

	static cpq_detail::flag_node_result flag_node(node_view at, node_view& next);

	static cpq_detail::exchange_link_result exchange_head_link(std::atomic<node_view>* link, node_view& expected, node_view& desired, std::uint32_t desiredVersion, std::uint8_t dbgLayer, node_view dbgNode);
	static cpq_detail::exchange_link_result exchange_node_link(std::atomic<node_view>* link, node_view& expected, node_view& desired, std::uint32_t desiredVersion, std::uint8_t dbgLayer, node_view dbgNode);

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
		expanded_node_view expected[LinkTowerHeight];
		expanded_node_view actual[LinkTowerHeight];
		expanded_node_view desired[LinkTowerHeight];

		const node_type* opNode = nullptr;

		cpq_detail::exchange_link_result result[LinkTowerHeight];

		uint32_t m_sequenceIndex = 0;
		uint8_t m_insertionCase = 0;
		std::thread::id m_threadId;
	};
	inline static thread_local recent_op_info t_recentOp = recent_op_info();
	inline static std::atomic_uint s_recentPushIx = 0;
	inline static std::atomic_uint s_recentPopIx = 0;
	inline static gdul::shared_ptr<recent_op_info[]> s_recentPushes = gdul::make_shared<recent_op_info[]>(1024);
	inline static gdul::shared_ptr<recent_op_info[]> s_recentPops = gdul::make_shared<recent_op_info[]>(1024);

	std::atomic<uint32_t> m_sequenceCounter = 0;
#endif

	// Also end
	node_type m_head;
};
template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::concurrent_priority_queue_impl()
	: pool_base<typename AllocationStrategy::template pool_type<Key, Value, LinkTowerHeight>::type>(LinkTowerHeight, LinkTowerHeight / std::thread::hardware_concurrency(), allocator_type())
	, m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();

	for (size_type i = 0; i < LinkTowerHeight; ++i) {
		m_head.m_next[i].store(&m_head, std::memory_order_relaxed);
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::concurrent_priority_queue_impl(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::allocator_type alloc)
	: pool_base<typename AllocationStrategy::template pool_type<Key, Value, LinkTowerHeight>::type>(LinkTowerHeight, LinkTowerHeight / std::thread::hardware_concurrency(), alloc)
	, m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();

	for (size_type i = 0; i < LinkTowerHeight; ++i) {
		m_head.m_next[i].store(&m_head, std::memory_order_relaxed);
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::~concurrent_priority_queue_impl()
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::push(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::input_type&& in)
{
	push_internal(std::move(in));
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::push(const typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::input_type& in)
{
	push_internal(in);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
template<class In>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::push_internal(In&& in)
{
	node_type* n(nullptr);

	if constexpr (!std::is_same_v<allocation_strategy, cpq_allocation_strategy_external>) {
		n = this->m_pool.get();
		n->m_kv = std::forward<In>(in);
		n->m_height = random_height(LinkTowerHeight);
	}
	else {
		n = in;
	}

#if defined GDUL_CPQ_DEBUG
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif

	if constexpr (std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_pool>) {
		while (!this->m_pool.guard(&concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_push, this, n));
	}
	else {
		while (!try_push(n));
	}

#if defined (GDUL_CPQ_DEBUG)
	t_recentOp.opNode = node;
	node->m_inserted = 1;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPushes[s_recentPushIx++ % s_recentPushes.item_count()] = t_recentOp;
#endif
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_pop(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::input_type& out)
{
	if constexpr (std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_pool>) {
		return this->m_pool.guard(&concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_pop_internal, this, out);
	}
	else {
		return try_pop_internal(out);
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_pop_internal(std::pair<Key, Value>& out)
{

#if defined GDUL_CPQ_DEBUG
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif

	bool flagged(false);
	bool delinked(false);

	node_type* mynode(nullptr);

	do {
		node_view frontNodeView(m_head.m_next[0].load(std::memory_order_seq_cst));
		mynode = frontNodeView;

		if (at_end(mynode)) {
			return false;
		}

		node_view_set frontSet{};
		node_view_set nextSet{};

		const std::uint8_t frontHeight(mynode->m_height);

		frontSet[0] = frontNodeView;

		load_set(frontSet, &m_head, 1, frontHeight);
		load_set(nextSet, mynode, 0, frontHeight);

		const cpq_detail::flag_node_result flagResult(flag_node(frontSet[0], nextSet[0]));

#if defined GDUL_CPQ_DEBUG
		if (flagResult == cpq_detail::flag_node_success) {
			mynode->m_flaggedVersion = nextSet[0].get_version();
		}
#endif
		if (flagResult == cpq_detail::flag_node_unexpected) {
			continue;
		}

		flagged = flagResult == cpq_detail::flag_node_success;
		delinked = try_delink_front(frontSet, nextSet, 1, frontHeight);

		if (flagged && !delinked) {

			delinked = has_been_delinked_by_other(mynode, frontSet[0], nextSet[0]);

			if (!delinked) {
				unflag_node(mynode, nextSet[0]);
			}
		}

#if defined GDUL_CPQ_DEBUG
		else if (delinked) {
			mynode->m_delinked = 1;
		}
#endif

	} while (!(flagged && delinked));

	out = std::move(mynode->m_kv);

	if constexpr (std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_pool>) {
		this->m_pool.recycle(mynode);
	}

#if defined GDUL_CPQ_DEBUG
	mynode->m_removed = 1;
	t_recentOp.opNode = mynode;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPops[s_recentPopIx++ % s_recentPops.item_count()] = t_recentOp;
#endif

	return true;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::clear_internal()
{
	node_view_set frontSet{};
	node_view_set nextSet{};

	node_type* frontNode(nullptr);

	do {
		frontSet[0] = (m_head.m_next[0].load(std::memory_order_seq_cst));
		frontNode = frontSet[0];

		if (at_end(frontNode)) {
			return;
		}

		load_set(frontSet, &m_head, 1);

		std::fill(std::begin(nextSet), std::end(nextSet), &m_head);

	} while (!try_delink_front(frontSet, nextSet, 1, LinkTowerHeight));


	if constexpr (std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_pool>) {
		while (!at_end(frontNode)) {
			node_type* const next(frontNode->m_next[0].load(std::memory_order_relaxed));
			this->m_pool.recycle(frontNode);
			frontNode = next;
		}
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::unsafe_reset()
{
	if constexpr (!std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_external>) {
		assert(empty() && "Bad call to unsafe_reset, there are still items in the list");
		this->m_pool.unsafe_reset();
	}

	std::for_each(std::begin(m_head.m_next), std::end(m_head.m_next), [this](std::atomic<node_view>& link) {link.store(&m_head, std::memory_order_relaxed); });
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node) const
{
	constexpr comparator_type comparator;

	const std::uint8_t nodeHeight(node->m_height);

	const key_type key(node->m_kv.first);

	bool beganProbing(false);
	for (std::uint8_t i = 0; i < LinkTowerHeight; ++i) {
		const std::uint8_t layer(LinkTowerHeight - i - 1);

		for (;;) {
			nextSet[layer] = atSet[layer].operator node_type * ()->m_next[layer].load(std::memory_order_seq_cst);
			node_type* const nextNode(nextSet[layer]);

			if (comparator(key, nextNode->m_kv.first)) {
				break;
			}

			if (at_end(nextNode)) {
				break;
			}

			// If we probe forward from HEAD node, we need to make sure the links we loaded before the link we
			// started probe from is not out-of-date
			if (!beganProbing) {
				const std::uint32_t probeVersion(nextSet[layer].get_version());
				for (std::uint8_t verifyIndex = layer + 1; verifyIndex < nodeHeight; ++verifyIndex) {

					if (m_head.m_next[verifyIndex].load(std::memory_order_relaxed).get_version() != nextSet[verifyIndex].get_version()) {
						return false;
					}
				}
				beganProbing = true;
			}

			atSet[layer] = nextSet[layer];
		}

		if (layer) {
			atSet[layer - 1] = atSet[layer];
		}
	}

	if (at_head(atSet[0])) {
		load_set(nextSet, &m_head, 1, nodeHeight);
	}

	return true;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline cpq_detail::flag_node_result concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::flag_node(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view at, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& next)
{
	const std::uint32_t expectedVersion(next.get_version());
	const std::uint32_t nextVersion(cpq_detail::version_add_one(at.get_version()));

	if (expectedVersion == nextVersion) {
		return cpq_detail::flag_node_compeditor;
	}

	if (!cpq_detail::in_range(expectedVersion, nextVersion)) {
		return cpq_detail::flag_node_unexpected;
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

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_delink_front(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& expectedFront, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& desiredFront, std::uint8_t versionOffset, std::uint8_t frontHeight)
{
	const std::uint8_t numUpperLayers(frontHeight - 1);
	const std::uint32_t baseLayerVersion(expectedFront[0].get_version());
	const std::uint32_t upperLayerNextVersion(cpq_detail::version_add_one(baseLayerVersion));

	for (std::uint8_t i = 0; i < numUpperLayers; ++i) {
		const std::uint8_t layer(frontHeight - 1 - i);

		const cpq_detail::exchange_link_result result(exchange_head_link(&m_head.m_next[layer], expectedFront[layer], desiredFront[layer], upperLayerNextVersion, layer, &m_head));

		if (result == cpq_detail::exchange_link_outside_range) {
			return false;
		}
		if (result == cpq_detail::exchange_link_success) {
			expectedFront[layer] = desiredFront[layer];
		}
	}

	const std::uint32_t bottomLayerNextVersion(cpq_detail::version_step(baseLayerVersion, versionOffset));

	perform_version_upkeep(frontHeight, baseLayerVersion, bottomLayerNextVersion, expectedFront);

	if (exchange_node_link(&m_head.m_next[0], expectedFront[0], desiredFront[0], bottomLayerNextVersion, 0, &m_head) == cpq_detail::exchange_link_success) {
		expectedFront[0] = desiredFront[0];
		return true;
	}
	return false;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::clear() noexcept
{
	if constexpr (std::is_same_v<allocation_strategy_identifier, cpq_detail::allocation_strategy_identifier_pool>) {
		this->m_pool.guard(&concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::clear_internal, this);
	}
	else {
		clear_internal();
	}

#if defined (GDUL_CPQ_DEBUG)
	s_recentPushIx = 0;
	s_recentPopIx = 0;
	std::fill(s_recentPushes.get(), s_recentPushes.get() + s_recentPushes.item_count(), recent_op_info());
	std::fill(s_recentPops.get(), s_recentPops.get() + s_recentPops.item_count(), recent_op_info());
#endif
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::empty() const noexcept
{
	return at_end(m_head.m_next[0].load(std::memory_order_relaxed));
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_link_to_node(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& atSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& expectedSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view node)
{
	node_type* const baseLayerNode(atSet[0]);

	if (exchange_node_link(&baseLayerNode->m_next[0], expectedSet[0], node, 0, 0, baseLayerNode) != cpq_detail::exchange_link_success) {
		return false;
	}

	link_to_node_upper(atSet, expectedSet, node);

	return true;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_link_to_head(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& next, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view node)
{
	const std::uint32_t baseLayerVersion(next[0].get_version());
	const std::uint32_t baseLayerNextVersion(cpq_detail::version_add_one(baseLayerVersion));

	perform_version_upkeep(node.operator const node_type * ()->m_height, baseLayerVersion, baseLayerNextVersion, next);

	if (exchange_node_link(&m_head.m_next[0], next[0], node, baseLayerNextVersion, 0, &m_head) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_head_upper(next, node, baseLayerNextVersion);

	return true;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_link_to_head_upper(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& expectedSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view node, std::uint32_t version)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		if (exchange_head_link(&m_head.m_next[layer], expectedSet[layer], node, version, layer, &m_head) == cpq_detail::exchange_link_outside_range) {
			break;
		}
	}
}


template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::link_to_node_upper(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& atSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& expectedSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view node)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		exchange_node_link(&atSet[layer].operator node_type * ()->m_next[layer], expectedSet[layer], node, expectedSet[layer].get_version(), layer, atSet[layer].operator node_type * ());
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_replace_front(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& frontSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& nextSet, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view node)
{
	nextSet[0] = node;

	const node_type* const toInsert(node);
	const node_type* const currentFront(frontSet[0]);

	const std::uint8_t frontHeight(currentFront->m_height);

	if (toInsert->m_height < frontHeight) {
		load_set(frontSet, &m_head, 1, frontHeight);
	}

	load_set(nextSet, currentFront, 1, frontHeight);

	if (try_delink_front(frontSet, nextSet, 2, frontHeight)) {

		try_link_to_head_upper(frontSet, node, frontSet[0].get_version());

		return true;
	}

	return false;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::load_set(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view_set& outSet, const node_type* at, std::uint8_t offset, std::uint8_t max)
{
	for (std::uint8_t i = offset; i < max; ++i) {
		outSet[i] = at->m_next[i].load(std::memory_order_seq_cst);
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::try_push(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* node)
{
	node_view_set atSet{};
	node_view_set nextSet{};

	atSet[LinkTowerHeight - 1] = &m_head;

	if (!prepare_insertion_sets(atSet, nextSet, node)) {
		return false;
	}

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

	// Inserting after unflagged node at least beyond head
	if (!is_flagged(nextSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 2;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	const node_view front(m_head.m_next[0].load(std::memory_order_relaxed));

	// Inserting after front node...
	if (atSet[0] == front) {

		// Front node is in the middle of deletion, attempt special case splice to head
		if (nextSet[0].get_version() == cpq_detail::version_add_one(front.get_version())) {

			// In case we are looking at front node, we need to make sure our view of head base layer version 
			// is younger than that of front base layer version. Else we might think it'd be okay with a
			// regular link at front node in the middle of deletion!
			atSet[0] = front;

#if defined (GDUL_CPQ_DEBUG)
			t_recentOp.m_insertionCase = 3;
#endif
			return try_replace_front(atSet, nextSet, node);
		}

		return false;
	}

	// Inserting after flagged node that has been supplanted in the middle of deletion
	if (exists_in_list(atSet[0], front)) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 4;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	// Cannot insert after a node that is not in list
	return false;
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::perform_version_upkeep(std::uint8_t aboveLayer, std::uint32_t baseVersion, std::uint32_t nextVersion, node_view_set& next)
{
	const size_type maxEntriesMultiPre(baseVersion / to_expected_list_size(LinkTowerHeight));
	const size_type maxEntriesMultiPost(nextVersion / to_expected_list_size(LinkTowerHeight));
	const bool delta(maxEntriesMultiPost - maxEntriesMultiPre);

	if (delta) {
		for (std::uint8_t i = aboveLayer; i < LinkTowerHeight; ++i) {

			node_view expected(next[i]);
			if (!expected.operator bool()) {
				expected = m_head.m_next[i].load(std::memory_order_relaxed);
			}

			const std::uint32_t linkVersion(expected.get_version());

			if (cpq_detail::in_range(linkVersion, baseVersion)) {

				const std::uint32_t versionDelta(cpq_detail::version_delta(linkVersion, baseVersion));

				// If one of the upper layer links has strayed to far from the current base layer version we need to 'push' 
				// it in to range again
				if (to_expected_list_size(LinkTowerHeight) < versionDelta) {
					const std::uint32_t recentVersion(cpq_detail::version_sub_one(baseVersion));
					const node_view desired(expected, recentVersion);

					m_head.m_next[i].compare_exchange_strong(expected, desired, std::memory_order_relaxed);
				}
			}
		}
	}
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::has_been_delinked_by_other(typename const concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* of, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view actual, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view triedReplacement) const
{
	const std::uint32_t triedVersion(triedReplacement.get_version());
	const std::uint32_t actualVersion(actual.get_version());
	const bool matchingVersion(triedVersion == actualVersion);

	// All good, some other actor did our replacement for us
	if (actual == triedReplacement &&
		matchingVersion) {
		return true;
	}

	const node_type* const ofNode(of);
	const node_type* fromNode(nullptr);

	if (!(actualVersion < triedVersion)) {
		fromNode = actual;
	}
	else {
		fromNode = m_head.m_next[0].load(std::memory_order_relaxed);
	}

	// If we can't find the node in the structure and version still belongs to us, it must be ours :)
	if (!exists_in_list(ofNode, fromNode)) {
		const std::uint32_t currentVersion(ofNode->m_next[0].load(std::memory_order_relaxed).get_version());
		const bool stillUnclaimed(currentVersion == triedVersion);

		return stillUnclaimed;
	}

	return false;
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline void concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::unflag_node(typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* at, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& next)
{
	const node_view desired(next, 0);
	at->m_next[0].compare_exchange_strong(next, desired, std::memory_order_relaxed);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline cpq_detail::exchange_link_result concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::exchange_head_link(std::atomic<typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view>* link, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& expected, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& desired, std::uint32_t desiredVersion, [[maybe_unused]] std::uint8_t dbgLayer, [[maybe_unused]] node_view dbgNode)
{
#if defined GDUL_CPQ_DEBUG
	t_recentOp.expected[dbgLayer] = expected;
#endif

	desired.set_version(desiredVersion);

	cpq_detail::exchange_link_result result(cpq_detail::exchange_link_success);

	do {
		const std::uint32_t expectedVersion(expected.get_version());

		if (expectedVersion == desiredVersion) {
			result = cpq_detail::exchange_link_other_link;
			break;
		}

		if (!cpq_detail::in_range(expectedVersion, desiredVersion)) {
			result = cpq_detail::exchange_link_outside_range;
			break;
		}
	} while (!link->compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed));


#if defined GDUL_CPQ_DEBUG
	t_recentOp.result[dbgLayer] = result;
	t_recentOp.actual[dbgLayer] = expected;
	t_recentOp.desired[dbgLayer] = desired;
	if (result && expected.get_version()) {
		dbgNode.operator node_type* ()->m_nextView[dbgLayer] = desired;
	}
#endif

	return result;
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline cpq_detail::exchange_link_result concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::exchange_node_link(std::atomic<typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view>* link, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& expected, typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view& desired, std::uint32_t desiredVersion, [[maybe_unused]] std::uint8_t dbgLayer, [[maybe_unused]] node_view dbgNode)
{
#if defined GDUL_CPQ_DEBUG
	t_recentOp.expected[dbgLayer] = expected;
#endif

	desired.set_version(desiredVersion);

	cpq_detail::exchange_link_result result(cpq_detail::exchange_link_outside_range);

	if (link->compare_exchange_strong(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
		result = cpq_detail::exchange_link_success;
	}

#if defined GDUL_CPQ_DEBUG
	t_recentOp.result[dbgLayer] = result;
	t_recentOp.actual[dbgLayer] = expected;
	t_recentOp.desired[dbgLayer] = desired;
	if (result && expected.get_version()) {
		dbgNode.operator node_type* ()->m_nextView[dbgLayer] = desired;
	}
#endif

	return result;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::exists_in_list(const typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* node, const typename concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* searchStart) const
{
	const compare_type comparator;

	// Linear base layer scan
	const key_type k(node->m_kv.first);
	const node_type* at(searchStart);

	for (;;) {
		if (at_end(at)) {
			break;
		}

		if (comparator(k, at->m_kv.first)) {
			break;
		}

		if (at == node) {
			return true;
		}

		at = at->m_next[0].load(std::memory_order_relaxed);
	}

	return false;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::at_end(const concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* n) const
{
	return n == &m_head;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::at_head(const concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_type* n) const
{
	return at_end(n);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class AllocationStrategy, class Compare>
inline bool concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::is_flagged(concurrent_priority_queue_impl<Key, Value, LinkTowerHeight, AllocationStrategy, Compare>::node_view n) const
{
	return n.get_version();
}

struct tl_container
{
	// https://codingforspeed.com/using-faster-psudo-random-generator-xorshift/

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
	}

	std::uint32_t x;
	std::uint32_t y;
	std::uint32_t z;
	std::uint32_t w;

}static thread_local t_rng;

static std::uint8_t random_height(std::uint8_t maxHeight)
{
	std::uint8_t result(1);

	for (std::uint8_t i = 0; i < maxHeight - 1; ++i, ++result) {
		if ((t_rng() % 4u) != 0) {
			break;
		}
	}

	return result;
}
static std::uint32_t version_delta(std::uint32_t from, std::uint32_t to)
{
	const std::uint32_t delta(to - from);
	return delta & Max_Version;
}
static std::uint32_t version_add_one(std::uint32_t from)
{
	const std::uint32_t next(from + 1);
	const std::uint32_t masked(next & Max_Version);
	const std::uint32_t zeroAdjust(!(bool)masked);

	return masked + zeroAdjust;
}
static std::uint32_t version_sub_one(std::uint32_t from)
{
	const std::uint32_t next(from - 1);
	const std::uint32_t zeroAdjust(!(bool)next);
	const std::uint32_t adjusted(next - zeroAdjust);

	return adjusted & Max_Version;
}
static std::uint32_t version_step(std::uint32_t from, std::uint8_t step)
{
	const std::uint32_t result(version_add_one(from));
	if (step == 1) {
		return result;
	}
	return version_add_one(result);
}
static bool in_range(std::uint32_t version, std::uint32_t inRangeOf)
{
	// Zero special case
	if (version != 0) {
		return version_delta(version, inRangeOf) < In_Range_Delta;
	}

	return true;
}
template <class Key, class Value, std::uint8_t LinkTowerHeight>
struct node
{
	class node_view
	{
	public:
		node_view() = default;
		node_view(node* n)
			: m_value((std::uintptr_t)(n))
		{
		}
		node_view(node* n, std::uint32_t version)
			:node_view(n)
		{
			set_version(version);
		}

		operator node* ()
		{
			return (node*)(m_value & cpq_detail::Pointer_Mask);
		}
		operator const node* () const
		{
			return (const node*)(m_value & cpq_detail::Pointer_Mask);
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
		operator bool() const
		{
			return operator const node * ();
		}
		std::uint32_t get_version() const
		{
			const std::uint64_t pointerValue(m_value & cpq_detail::Version_Mask);
			const std::uint64_t lower(pointerValue & cpq_detail::Bottom_Bits);
			const std::uint64_t upper(pointerValue >> 45);
			const std::uint64_t conc(lower + upper);

			return (std::uint32_t)conc;
		}
		void set_version(std::uint32_t v)
		{
			const std::uint64_t v64(v);
			const std::uint64_t lower(v64 & cpq_detail::Bottom_Bits);
			const std::uint64_t upper((v64 << 45) & ~cpq_detail::Pointer_Mask);
			const std::uint64_t versionValue(upper | lower);
			const std::uint64_t pointerValue(m_value & cpq_detail::Pointer_Mask);

			m_value = (versionValue | pointerValue);
		}
		union
		{
			const node* m_nodeView;
			std::uintptr_t m_value;
		};
	};

	node() : m_kv(), m_next{}, m_height(LinkTowerHeight) {}
	node(std::pair<Key, Value>&& item) : m_kv(std::move(item)), m_next{}, m_height(LinkTowerHeight) {}
	node(const std::pair<Key, Value>& item) : m_kv(item), m_next{}, m_height(LinkTowerHeight){}

	std::atomic<node_view> m_next[LinkTowerHeight]{};
#if defined (GDUL_CPQ_DEBUG)
	const node* m_nextView[LinkTowerHeight]{};
	std::uint32_t m_flaggedVersion = 0;
	std::uint8_t m_delinked = 0;
	std::uint8_t m_removed = 0;
	std::uint8_t m_inserted = 0;
#endif

	std::pair<Key, Value> m_kv;
	std::uint8_t m_height;
};
template <class Pool>
struct pool_base
{
	pool_base(size_type blockSize, size_type tlCacheSize, typename Pool::allocator_type alloc)
		: m_pool((typename Pool::size_type)blockSize, (typename Pool::size_type)tlCacheSize, alloc)
	{
	}
protected:
	Pool m_pool;
};

template <>
struct pool_base<void>
{
	template <class ...Args>
	pool_base(Args&&...) {}
};
}

template <class Allocator>
struct cpq_allocation_strategy_pool
{
	using allocator_type = Allocator;
	using identifier = cpq_detail::allocation_strategy_identifier_pool;

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct input_type
	{
		using type = std::pair<Key, Value>;
	};

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct pool_type
	{
		using type = concurrent_guard_pool<cpq_detail::node<Key, Value, LinkTowerHeight>, allocator_type>;
	};
};

template <class Allocator>
struct cpq_allocation_strategy_scratch
{
	using allocator_type = Allocator;
	using identifier = cpq_detail::allocation_strategy_identifier_scratch;

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct input_type
	{
		using type = std::pair<Key, Value>;
	};

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct pool_type
	{
		using type = concurrent_scratch_pool<cpq_detail::node<Key, Value, LinkTowerHeight>, allocator_type>;
	};
};

struct cpq_allocation_strategy_external
{
	using allocator_type = void;
	using identifier = cpq_detail::allocation_strategy_identifier_external;

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct input_type
	{
		using type = cpq_detail::node<Key, Value, LinkTowerHeight>;
	};

	template <class Key, class Value, std::uint8_t LinkTowerHeight>
	struct pool_type
	{
		using type = void;
	};
};
}
#pragma warning(pop)