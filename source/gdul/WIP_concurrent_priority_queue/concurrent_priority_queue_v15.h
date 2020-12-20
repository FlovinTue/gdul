#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <random>
#include <gdul/WIP_concurrent_priority_queue/concurrent_scratch_pool.h>
#include <gdul/memory/concurrent_guard_pool.h>

//#define GDUL_CPQ_DEBUG
#if defined GDUL_CPQ_DEBUG
#include <cassert>
#include <utility>
#include <thread>
#include "../Testers/Common/util.h"
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#endif

// ---- to delete -------------
#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)
// ----------------------------

#define GDUL_CPQ_GUARDED_POOL
#define GDUL_CPQ_HEAD_VAR m_fauxHead

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
constexpr std::uint8_t calc_max_height(size_type TypicalSizeHint)
{
	const std::uint8_t naturalHeight((std::uint8_t)log2ceil(TypicalSizeHint));
	const std::uint8_t croppedHeight(naturalHeight / 2 + bool(naturalHeight % 2));

	return croppedHeight;
}

constexpr std::uintptr_t Bottom_Bits = ~(std::uintptr_t(std::numeric_limits<std::uintptr_t>::max()) << 3);
constexpr std::uintptr_t Pointer_Mask = (std::numeric_limits<std::uintptr_t>::max() >> 16) - Bottom_Bits;
constexpr std::uintptr_t Version_Mask = ~Pointer_Mask;
constexpr std::uint32_t Max_Version = (std::numeric_limits<std::uint32_t>::max() >> (16 - 3));
constexpr std::uint32_t In_Range_Delta = Max_Version / 2;

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

static std::uint8_t random_height(size_type);

constexpr std::uint32_t version_delta(std::uint32_t from, std::uint32_t to)
{
	const std::uint32_t delta(to - from);
	return delta & Max_Version;
}
constexpr std::uint32_t version_add_one(std::uint32_t from)
{
	const std::uint32_t next(from + 1);
	const std::uint32_t masked(next & Max_Version);
	const std::uint32_t zeroAdjust(!(bool)masked);

	return masked + zeroAdjust;
}
constexpr std::uint32_t version_sub_one(std::uint32_t from)
{
	const std::uint32_t next(from - 1);
	const std::uint32_t zeroAdjust(!(bool)next);
	const std::uint32_t adjusted(next - zeroAdjust);

	return adjusted & Max_Version;
}
constexpr std::uint32_t version_step(std::uint32_t from, std::uint8_t step)
{
	const std::uint32_t result(version_add_one(from));
	if (step == 1) {
		return result;
	}
	return version_add_one(result);
}
constexpr bool in_range(std::uint32_t version, std::uint32_t inRangeOf)
{
	// Zero special case
	if (version != 0) {
		return version_delta(version, inRangeOf) < In_Range_Delta;
	}

	return true;
}

template <class Key, class Value, size_type NodeHeight>
struct node
{
	node() : m_kv(), m_next{}, m_height(NodeHeight) {}
	node(std::pair<Key, Value>&& item) : m_kv(std::move(item)), m_next{}, m_height(NodeHeight) {}
	node(const std::pair<Key, Value>& item) : m_kv(item), m_next{}, m_height(NodeHeight){}


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
		operator bool() const
		{
			return operator const node * ();
		}
		std::uint32_t get_version() const
		{
			const std::uint64_t pointerValue(m_value & Version_Mask);
			const std::uint64_t lower(pointerValue & Bottom_Bits);
			const std::uint64_t upper(pointerValue >> 45);
			const std::uint64_t conc(lower + upper);

			return (std::uint32_t)conc;
		}
		void set_version(std::uint32_t v)
		{
			const std::uint64_t v64(v);
			const std::uint64_t lower(v64 & Bottom_Bits);
			const std::uint64_t upper((v64 << 45) & ~Pointer_Mask);
			const std::uint64_t versionValue(upper | lower);
			const std::uint64_t pointerValue(m_value & Pointer_Mask);

			m_value = (versionValue | pointerValue);
		}
		union
		{
			const node* m_nodeView;
			std::uintptr_t m_value;
		};
	};

	std::atomic<node_view> m_next[NodeHeight]{};
	std::uint8_t m_height;
#if defined (GDUL_CPQ_DEBUG)
	const node* m_nextView[NodeHeight]{};
	std::uint32_t m_flaggedVersion = 0;
	std::uint8_t m_delinked = 0;
	std::uint8_t m_removed = 0;
	std::uint8_t m_inserted = 0;
#endif

	std::pair<Key, Value> m_kv;
};
}
/// <summary>
/// A concurrency safe lock-free priority queue based on skip list design
/// </summary>
/// <param name="Key">Key type</param>
/// <param name="Value">Value type</param>
/// <param name="TypicalSizeHint">How many items is expected to be in the list at any one time</param>
/// <param name="Compare">Comparator to decide node ordering</param>
template <
	class Key,
	class Value,
	typename cpq_detail::size_type TypicalSizeHint = 512,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::uint8_t>
>
class concurrent_priority_queue
{
	static constexpr std::uint8_t Link_Tower_Height = cpq_detail::calc_max_height(TypicalSizeHint);
public:
	using key_type = Key;
	using value_type = Value;
	using compare_type = Compare;
	using node_type = cpq_detail::node<Key, Value, Link_Tower_Height>;
	using size_type = typename cpq_detail::size_type;
	using comparator_type = Compare;
	using allocator_type = Allocator;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_priority_queue();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="alloc">Allocator passed to node pool</param>
	concurrent_priority_queue(Allocator alloc);

	/// <summary>
	/// Destructor
	/// </summary>
	/// <returns></returns>
	~concurrent_priority_queue() noexcept;

	/// <summary>
	/// Enqueue an item
	/// </summary>
	/// <param name="in">Item to be enqueued</param>
	void push(const std::pair<Key, Value>& in);

	/// <summary>
	/// Enqueue an item
	/// </summary>
	/// <param name="in">Item to be enqueued using move semantics</param>
	void push(std::pair<Key, Value>&& in);

	/// <summary>
	/// Attempt to dequeue the top item
	/// </summary>
	/// <param name="out">Item to be written. Will be remain unmodified in case of failure</param>
	/// <returns>true on success, false otherwise</returns>
	bool try_pop(std::pair<Key, Value>& out);

	/// <summary>
	/// Clear items from list
	/// </summary>
	void clear();

	/// <summary>
	/// Query for items
	/// </summary>
	/// <returns>True if any item is present</returns>
	bool empty() const noexcept;

	/// <summary>
	/// Reset the internal node scratch pool. This must be done periodically.
	/// Assumes no concurrent inserts and an empty structure
	/// </summary>
	void unsafe_reset_scratch_pool();

private:
	template <class In>
	void push_internal(In&& in);
	bool try_pop_internal(std::pair<Key, Value>& out);

	void clear_internal();

	using node_view = typename node_type::node_view;
	using node_view_set = node_view[Link_Tower_Height];

	bool find_node(const node_type* node) const;
	bool prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node);

	bool at_end(const node_type*) const;
	bool at_head(const node_type*) const;

	bool try_push(node_type* node);

	bool try_delink_front(node_view_set& expectedFront, node_view_set& desiredFront, std::uint8_t versionOffset, std::uint8_t frontHeight);
	bool try_replace_front(node_view_set& current, node_view_set& next, node_view node);

	bool try_link_to_head(node_view_set& next, node_view node);
	void try_link_to_head_upper(node_view_set& next, node_view node, std::uint32_t version);

	bool check_for_delink_helping(node_view of, node_view actual, node_view triedReplacement);

	void perform_version_upkeep(std::uint8_t aboveLayer, std::uint32_t baseVersion, std::uint32_t nextVersion, node_view_set& next);

	template <class At>
	static void load_set(node_view_set& outSet, At* at, std::uint8_t offset = 0, std::uint8_t max = Link_Tower_Height);
	static void unflag_node(node_type* at, node_view& next);
	static void try_link_to_node_upper(node_view_set& at, node_view_set& next, node_view node);
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
		expanded_node_view expected[Link_Tower_Height];
		expanded_node_view actual[Link_Tower_Height];
		expanded_node_view desired[Link_Tower_Height];

		const node_type* opNode = nullptr;

		cpq_detail::exchange_link_result result[Link_Tower_Height];

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
	struct faux_head_node
	{
		struct alignas(std::hardware_destructive_interference_size) link : std::atomic<node_view> {};

		struct link_pack
		{
			std::atomic<node_view>& operator[](size_type index)
			{
				if (index) {
					return m_topLayers[index - 1];
				}
				return m_baseLayer;
			}
			const std::atomic<node_view>& operator[](size_type index) const
			{
				if (index) {
					return m_topLayers[index - 1];
				}
				return m_baseLayer;
			}

			std::atomic<node_view> m_topLayers[Link_Tower_Height - 1];
			alignas(64)std::atomic<node_view> m_baseLayer;
		};


		link m_next[Link_Tower_Height];
		//link_pack m_next;
	};

#if defined GDUL_CPQ_GUARDED_POOL
	guarded_pool<node_type, Allocator> m_nodePool;
#else
	concurrent_scratch_pool<node_type, Allocator> m_nodePool;
#endif
	faux_head_node m_fauxHead;

	// Also end
	node_type m_head;
};
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::concurrent_priority_queue()
	: concurrent_priority_queue(Allocator())
{
}
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::concurrent_priority_queue(Allocator alloc)
#if defined GDUL_CPQ_GUARDED_POOL
	: m_nodePool(TypicalSizeHint, TypicalSizeHint / std::thread::hardware_concurrency(), alloc)
#else
	: m_nodePool(TypicalSizeHint, alloc)
#endif
	, m_fauxHead()
	, m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();

	for (size_type i = 0; i < Link_Tower_Height; ++i) {
		GDUL_CPQ_HEAD_VAR.m_next[i].store(&m_head, std::memory_order_relaxed);
	}

}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::~concurrent_priority_queue()
{
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::push(std::pair<Key, Value>&& in)
{
	push_internal(std::move(in));
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::push(const std::pair<Key, Value>& in)
{
	push_internal(in);
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
template<class In>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::push_internal(In&& in)
{
	node_type* const n(m_nodePool.get());
	n->m_kv = std::forward<In>(in);
	n->m_height = cpq_detail::random_height(Link_Tower_Height);

#if defined GDUL_CPQ_DEBUG
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
}
#endif
#if defined GDUL_CPQ_GUARDED_POOL
	while (!m_nodePool.guard(&concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_push, this, n));
#else
	while (!try_push(n));
#endif
#if defined (GDUL_CPQ_DEBUG)
	t_recentOp.opNode = node;
	node->m_inserted = 1;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPushes[s_recentPushIx++ % s_recentPushes.item_count()] = t_recentOp;
#endif
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_pop(std::pair<Key, Value>& out)
{
#if defined GDUL_CPQ_GUARDED_POOL
	return m_nodePool.guard(&concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_pop_internal, this, out);
#else
	return try_pop_internal(out);
#endif
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_pop_internal(std::pair<Key, Value>& out)
{

#if defined GDUL_CPQ_DEBUG
	for (auto itr = std::begin(t_recentOp.result); itr != std::end(t_recentOp.result); ++itr) {
		*itr = cpq_detail::exchange_link_null_value;
	}
#endif

	cpq_detail::flag_node_result flagResult(cpq_detail::flag_node_unexpected);
	bool deLinked(false);

	node_type* frontNode(nullptr);

	do {
		node_view frontNodeView(GDUL_CPQ_HEAD_VAR.m_next[0].load(std::memory_order_seq_cst));
		frontNode = frontNodeView;

		if (at_end(frontNode)) {
			return false;
		}

		node_view_set frontSet{};
		node_view_set nextSet{};

		const std::uint8_t frontHeight(frontNode->m_height);

		frontSet[0] = frontNodeView;

		load_set(frontSet, &GDUL_CPQ_HEAD_VAR, 1, frontHeight);
		load_set(nextSet, frontNode, 0, frontHeight);

		flagResult = flag_node(frontSet[0], nextSet[0]);

#if defined GDUL_CPQ_DEBUG
		if (flagResult == cpq_detail::flag_node_success) {
			frontNode->m_flaggedVersion = nextSet[0].get_version();
		}
#endif
		if (flagResult == cpq_detail::flag_node_unexpected) {
			continue;
		}

		deLinked = try_delink_front(frontSet, nextSet, 1, frontHeight);

		if (!deLinked && flagResult == cpq_detail::flag_node_success) {

			deLinked = check_for_delink_helping(frontNodeView, frontSet[0], nextSet[0]);

			if (!deLinked) {
				unflag_node(frontNode, nextSet[0]);
			}
		}

#if defined GDUL_CPQ_DEBUG
		else if (deLinked) {
			frontNode->m_delinked = 1;
		}
#endif

	} while (flagResult != cpq_detail::flag_node_success || !deLinked);

	out = std::move(frontNode->m_kv);

#if defined GDUL_CPQ_GUARDED_POOL
	m_nodePool.recycle(frontNode);
#endif

#if defined GDUL_CPQ_DEBUG
	frontNode->m_removed = 1;
	t_recentOp.opNode = frontNode;
	t_recentOp.m_sequenceIndex = m_sequenceCounter++;
	t_recentOp.m_threadId = std::this_thread::get_id();
	s_recentPops[s_recentPopIx++ % s_recentPops.item_count()] = t_recentOp;
#endif

	return true;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::clear_internal()
{
	node_view_set frontSet{};
	node_view_set nextSet{};

	node_type* frontNode(nullptr);

	do {
		frontSet[0] = (GDUL_CPQ_HEAD_VAR.m_next[0].load(std::memory_order_seq_cst));
		frontNode = frontSet[0];

		if (at_end(frontNode)) {
			return;
		}

		load_set(frontSet, &GDUL_CPQ_HEAD_VAR, 1);

		std::fill(std::begin(nextSet), std::end(nextSet), &m_head);

	} while (!try_delink_front(frontSet, nextSet, 1, Link_Tower_Height));


#if defined ( GDUL_CPQ_GUARDED_POOL )
	while (!at_end(frontNode)) {
		node_type* const next(frontNode->m_next[0].load(std::memory_order_relaxed));
		m_nodePool.recycle(frontNode);
		frontNode = next;
	}
#endif

}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::unsafe_reset_scratch_pool()
{
#if !defined GDUL_CPQ_GUARDED_POOL
	assert(empty() && "Bad call to unsafe_reset_scratch_pool, there are still items in the list");
	m_nodePool.unsafe_reset();
#endif
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::prepare_insertion_sets(node_view_set& atSet, node_view_set& nextSet, const node_type* node)
{
	// For future, may optimize here. If descending to bottom layer at HEAD, then we may detect if we're at front node
	// (To avoid reloading during insertion case 3)

	constexpr comparator_type comparator;

	const std::uint8_t nodeHeight(node->m_height);

	atSet[Link_Tower_Height - 1] = &m_head;

	std::atomic<node_view>* atLink(&GDUL_CPQ_HEAD_VAR.m_next[Link_Tower_Height - 1]);

	const key_type key(node->m_kv.first);

	bool beganProbing(false);
	for (std::uint8_t i = 0; i < Link_Tower_Height; ++i) {
		const std::uint8_t layer(Link_Tower_Height - i - 1);

		for (;;) {
			nextSet[layer] = atLink->load(std::memory_order_seq_cst);
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

					if (GDUL_CPQ_HEAD_VAR.m_next[verifyIndex].load(std::memory_order_relaxed).get_version() != nextSet[verifyIndex].get_version()) {
						return false;
					}
				}
				beganProbing = true;
			}

			atSet[layer] = nextSet[layer];
			atLink = &nextNode->m_next[layer];
		}

		if (layer) {
			atSet[layer - 1] = atSet[layer];

			// Todo remove this
			if (!at_head(atSet[layer])) {
				atLink = &atSet[layer].operator node_type * ()->m_next[layer - 1];
			}
			else {
				atLink = &GDUL_CPQ_HEAD_VAR.m_next[layer - 1];
			}
		}
	}

	if (at_head(atSet[0])) {
		load_set(nextSet, &GDUL_CPQ_HEAD_VAR, 1, nodeHeight);
	}

	return true;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline cpq_detail::flag_node_result concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::flag_node(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view at, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& next)
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

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_delink_front(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& expectedFront, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& desiredFront, std::uint8_t versionOffset, std::uint8_t frontHeight)
{
	const std::uint8_t numUpperLayers(frontHeight - 1);
	const std::uint32_t baseLayerVersion(expectedFront[0].get_version());
	const std::uint32_t upperLayerNextVersion(cpq_detail::version_add_one(baseLayerVersion));

	for (std::uint8_t i = 0; i < numUpperLayers; ++i) {
		const std::uint8_t layer(frontHeight - 1 - i);

		const cpq_detail::exchange_link_result result(exchange_head_link(&GDUL_CPQ_HEAD_VAR.m_next[layer], expectedFront[layer], desiredFront[layer], upperLayerNextVersion, layer, &m_head));

		if (result == cpq_detail::exchange_link_outside_range) {
			return false;
		}
		if (result == cpq_detail::exchange_link_success) {
			expectedFront[layer] = desiredFront[layer];
		}
	}

	const std::uint32_t bottomLayerNextVersion(cpq_detail::version_step(baseLayerVersion, versionOffset));

	perform_version_upkeep(frontHeight, baseLayerVersion, bottomLayerNextVersion, expectedFront);

	if (exchange_node_link(&GDUL_CPQ_HEAD_VAR.m_next[0], expectedFront[0], desiredFront[0], bottomLayerNextVersion, 0, &m_head) == cpq_detail::exchange_link_success) {
		expectedFront[0] = desiredFront[0];
		return true;
	}
	return false;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::clear()
{
	std::pair<Key, Value> dump;
#if defined GDUL_CPQ_GUARDED_POOL
	m_nodePool.guard(&concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::clear_internal, this);
#else
	clear_internal();
#endif

#if defined (GDUL_CPQ_DEBUG)
	s_recentPushIx = 0;
	s_recentPopIx = 0;
	std::fill(s_recentPushes.get(), s_recentPushes.get() + s_recentPushes.item_count(), recent_op_info());
	std::fill(s_recentPops.get(), s_recentPops.get() + s_recentPops.item_count(), recent_op_info());
#endif
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::empty() const noexcept
{
	return at_end(GDUL_CPQ_HEAD_VAR.m_next[0].load(std::memory_order_relaxed));
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_link_to_node(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view node)
{
	node_type* const baseLayerNode(atSet[0]);

	if (exchange_node_link(&baseLayerNode->m_next[0], expectedSet[0], node, 0, 0, baseLayerNode) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_node_upper(atSet, expectedSet, node);

	return true;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_link_to_head(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& next, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view node)
{
	const std::uint32_t baseLayerVersion(next[0].get_version());
	const std::uint32_t baseLayerNextVersion(cpq_detail::version_add_one(baseLayerVersion));

	perform_version_upkeep(node.operator const node_type * ()->m_height, baseLayerVersion, baseLayerNextVersion, next);

	if (exchange_node_link(&GDUL_CPQ_HEAD_VAR.m_next[0], next[0], node, baseLayerNextVersion, 0, &m_head) != cpq_detail::exchange_link_success) {
		return false;
	}

	try_link_to_head_upper(next, node, baseLayerNextVersion);

	return true;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_link_to_head_upper(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view node, std::uint32_t version)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		if (exchange_head_link(&GDUL_CPQ_HEAD_VAR.m_next[layer], expectedSet[layer], node, version, layer, &m_head) == cpq_detail::exchange_link_outside_range) {
			break;
		}
	}
}


template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_link_to_node_upper(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& atSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& expectedSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view node)
{
	const std::uint8_t height(node.operator node_type * ()->m_height);

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		exchange_node_link(&atSet[layer].operator node_type * ()->m_next[layer], expectedSet[layer], node, expectedSet[layer].get_version(), layer, atSet[layer].operator node_type * ());
	}
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_replace_front(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& frontSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& nextSet, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view node)
{
	nextSet[0] = node;

	const node_type* const toInsert(node);
	const node_type* const currentFront(frontSet[0]);

	const std::uint8_t frontHeight(currentFront->m_height);

	if (toInsert->m_height < frontHeight) {
		load_set(frontSet, &GDUL_CPQ_HEAD_VAR, 1, frontHeight);
	}

	load_set(nextSet, currentFront, 1, frontHeight);

	if (try_delink_front(frontSet, nextSet, 2, frontHeight)) {

		try_link_to_head_upper(frontSet, node, frontSet[0].get_version());

		return true;
	}

	return false;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
template <class At>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::load_set(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view_set& outSet, At* at, std::uint8_t offset, std::uint8_t max)
{
	for (std::uint8_t i = offset; i < max; ++i) {
		outSet[i] = at->m_next[i].load(std::memory_order_seq_cst);
	}
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::try_push(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_type* node)
{
	node_view_set atSet{};
	node_view_set nextSet{};

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

	// Inserting after unflagged node atleast beyond head
	if (!nextSet[0].get_version()) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 2;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	const node_view front(GDUL_CPQ_HEAD_VAR.m_next[0].load(std::memory_order_seq_cst));

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
	if (find_node(atSet[0])) {
#if defined (GDUL_CPQ_DEBUG)
		t_recentOp.m_insertionCase = 4;
#endif
		return try_link_to_node(atSet, nextSet, node);
	}

	// Cannot insert after a node that is not in list
	return false;
}
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::perform_version_upkeep(std::uint8_t aboveLayer, std::uint32_t baseVersion, std::uint32_t nextVersion, node_view_set& next)
{
	const std::uint32_t maxEntriesMultiPre(baseVersion / TypicalSizeHint);
	const std::uint32_t maxEntriesMultiPost(nextVersion / TypicalSizeHint);
	const bool delta(maxEntriesMultiPost - maxEntriesMultiPre);

	if (delta) {
		for (std::uint8_t i = aboveLayer; i < Link_Tower_Height; ++i) {

			node_view expected(next[i]);
			if (!expected.operator bool()) {
				expected = GDUL_CPQ_HEAD_VAR.m_next[i].load(std::memory_order_relaxed);
			}

			const std::uint32_t linkVersion(expected.get_version());

			if (cpq_detail::in_range(linkVersion, baseVersion)) {

				const std::uint32_t versionDelta(cpq_detail::version_delta(linkVersion, baseVersion));

				if (TypicalSizeHint < versionDelta) {
					const std::uint32_t recentVersion(cpq_detail::version_sub_one(baseVersion));
					const node_view desired(expected, recentVersion);

					GDUL_CPQ_HEAD_VAR.m_next[i].compare_exchange_strong(expected, desired, std::memory_order_relaxed);
				}
			}
		}
	}
}
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::check_for_delink_helping(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view of, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view actual, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view triedReplacement)
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
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline void concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::unflag_node(typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_type* at, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& next)
{
	const node_view desired(next, 0);
	at->m_next[0].compare_exchange_strong(next, desired, std::memory_order_relaxed);
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline cpq_detail::exchange_link_result concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::exchange_head_link(std::atomic<typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view>* link, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& desired, std::uint32_t desiredVersion, [[maybe_unused]] std::uint8_t dbgLayer, [[maybe_unused]] node_view dbgNode)
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
template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline cpq_detail::exchange_link_result concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::exchange_node_link(std::atomic<typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view>* link, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& expected, typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_view& desired, std::uint32_t desiredVersion, [[maybe_unused]] std::uint8_t dbgLayer, [[maybe_unused]] node_view dbgNode)
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

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::find_node(const typename concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_type* node) const
{
	// Might also be able to take a source node argument, so as to skip loading head here.. 

	const compare_type comparator;

	// Linear base layer scan
	const key_type k(node->m_kv.first);
	const node_type* at(GDUL_CPQ_HEAD_VAR.m_next[0].load(std::memory_order_relaxed));

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

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::at_end(const concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_type* n) const
{
	return n == &m_head;
}

template<class Key, class Value, typename cpq_detail::size_type TypicalSizeHint, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::at_head(const concurrent_priority_queue<Key, Value, TypicalSizeHint, Compare, Allocator>::node_type* n) const
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
static std::uint8_t random_height(size_type maxHeight)
{
	std::uint8_t result(1);

	for (std::uint8_t i = 0; i < maxHeight - 1; ++i, ++result) {
		if ((t_rng() % 4) != 0) {
			break;
		}
	}

	return result;
}
}
}

#pragma warning(pop)





// Idea, memory_reclaimer ... Or
// memory_reclaimer combined with memory_pool. Or something.
// Uses something like qsbr. Or adapted. 


// How would memory_reclaimer work? take delegate for on_reclaim? 
// Threads would have to check in, but then what? Or ? 



// check_in()
// log  thread local checkins
// after a set amount of checkins push global
// nah.

// So what we want is probably to keep it somewhat generic. 
// Make threads check in after each.. Op? Try? After probing forward from head?
// BUT keep it *mostly* thread local. So something like, sync local 100 then try sync global...
// Not sure about the details of this, but yes this should probably be the base concept.
// Annoying if one thread updates its local sync once and then not again. This would make for a
// less than ideal situation.

// How about thread_check_in()
// And then put responsibility on the user to make sure this is called regularly. Like, once per frame.







// Idea
// Go for qsbr. Use the previous idea of tl batching recycles along with submitting the entire list as a new set?  