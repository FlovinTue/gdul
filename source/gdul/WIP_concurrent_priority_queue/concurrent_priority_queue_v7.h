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

		inline node_view load() const
		{
			return m_value.load(std::memory_order_relaxed);
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

	bool prepare_insertion_sets(node_view_set& outCurrent, node_view_set& outNext, Key key);

	void unlink_successors(node_type* of, const node_type* expecting);

	static bool try_link(node_type* node, node_view_set& current, node_view_set& next);

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

			frontSet[atLayer] = m_head.m_next[atLayer].load();
		}

		node_type* const frontNode(frontSet[0]);
		if (frontNode == &m_head) {
			return false;
		}

		const std::uint8_t frontHeight(frontNode->m_height);

		for (std::uint8_t i = 0; i < frontHeight; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);

			replacementSet[atLayer] = frontNode->m_next[atLayer].load();
			replacementSet[atLayer].set_version(frontSet[atLayer].get_version() + 1);
		}

		for (std::uint8_t i = 0; i < frontHeight - 1; ++i) {
			const std::uint8_t atLayer(frontHeight - 1 - i);

			if (frontSet[atLayer] == replacementSet[atLayer]) {
				continue;
			}

		retry:
			if (m_head.m_next[atLayer].m_value.compare_exchange_strong(frontSet[atLayer].m_value, replacementSet[atLayer].m_value, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				continue;
			}
			if (frontSet[atLayer].operator node_type*() == frontNode) {
				goto retry;
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

	// The base layer is what matters. The rest is just search-juice
	{
		std::uintptr_t expected(outNext[0].m_value);

		// Sanity check
		assert((node_type*)outNext[0] && "Expected value at next");

		if (!outCurrent[0].operator node_type * ()->m_next[0].m_value.compare_exchange_weak(expected, desired, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			return false;
		}
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
		if (prepare_insertion_sets(current, next, node->m_kv.first) &&
			try_link(node, current, next)) {
			break;
		}
	}
}

template<class Key, class Value, class Compare, class Allocator>
inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::prepare_insertion_sets(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outCurrent, typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_view_set& outNext, Key key)
{
	std::uninitialized_fill(std::begin(outCurrent), std::end(outCurrent), node_view(&m_head));

	for (size_type i = 0; i < cpq_detail::Max_Node_Height; ++i) {
		const size_type atLayer(cpq_detail::Max_Node_Height - i - 1);

		// So upper layers contain references to stale nodes.. Would that be inserters or deleters' responsibility..  ? 
		// Well deleters are the ones that repair stale node links. Hmm. So. If the deletion de-linking works as it should
		// it'll de-link from the top and make sure that there are no upper layer links left in the case it succeeds in de-linking bottom layer. *SHOULD*.

		// So, in case we were able to *guarantee* that deleters always successfully deleted all links to the deleted node

		// Could we perhaps end up linking to a deleted node somehow, when de-linking? Like, attempting to delink 1, stalling
		// and then having 2 be deleted, then linking to 2. ?

		// I suppose if an inserter attempted to link 2 layers at head, beginning at bottom and then stalling would result in a situation
		// where a stale node could be linked at upper layer. Hmm yeah.

		// What about

		// HEAD----------------**1**----------------2-----------------------------------------END
		// HEAD----------------**1**----------------2--------------------3--------------------END

		// Deleting 1 with a stale link
		for (;;) {
			node_type* const currentNode(outCurrent[atLayer]);
			outNext[atLayer] = node_view(currentNode->m_next[atLayer].m_value.load(std::memory_order_relaxed));
			node_type* const nextNode(outNext[atLayer]);

			if (!nextNode) {
				return false;
			}

			if (!m_comparator(nextNode->m_kv.first, key)) {
				break;
			}

			outCurrent[atLayer] = outNext[atLayer];
		}

		if (atLayer) {
			outCurrent[atLayer - 1] = outCurrent[atLayer];
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














// So..
//
// * Load first
// * Load second
// * cas head from first to second
//
// The danger is if an inserter decides to link to the deleted node. The inserter should have a way of verifying
// it didn't insert in deletion territory. Putting the synchronization burden on the deleter will only cause further 
// congestion in an already quite congested area. We don't want to store on the front node cacheline, since that'll
// cause massive congestion.
// 
// And what about multiple inserters inserting in the deleted node in sequence. It pretty much makes it out of the question 
// to have the deleter restore the proper order.

// I guess a deletion index could be kept in head and this could be checked by the inserter.. 









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

	// The idea of synchronizing with deleters is that *all* nodes from m_head -> (m_head.m_version - m_recentRefs) will be claimed for
		// deletion. If an inserter is able to synchronize with this value securely, and abort in case of delete interference,
		// that would mean inserters only have other inserters to contend with..... In which case there is no danger (concurrent insertion
		// is pretty simple, on it's own).

		// The way this could work would be to load recentRefs - > track steps forward (beyond current delete-claimed)-> link in ->
		// reload head to make sure link was not attached to deleted node.



		// So. The issue is of course with an inserter loading and inserting





	// So, the deleter will attempt to splice m_next onto head rather than the previous node.
	// If several deleters operate concurrently only the last one will succeed. 












	// SO. Dealing with concurrent insertion first. 

	// Insertions will begin at bottom. -- A node is always locateable if present at bottom layer, so as soon as linking happens at
	// bottom, it will exist in structure. 
	// Linking should continue upward from there (?)
	// What kind of conflicts may happen here with other inserters?
	// Bottom insertion is already solved.(Currently!). 

	// If doing it simple, two layers:

	// Searches will begin at top layer,
	// locating all the previous nodes of inserting node height.
	// This should be simple enough with the current scheme, since we are NOT taking garbage collection in to account.

	// For simplicities sake, let's say bottom layer inserts fine.
	// For the second, in case of failure to link, and not considering deleters, another inserter might have linked there before. 
	// What then? If the node is located on the left side then it should be fine. It'll just be as if the current node was a 1 layer node.
	// If the node is located on the right side, that would mean the current node has been superceeded somehow. (Is this possible?)


	// 1------------------------------------------------------------END
	// 1------------------2--------------------3--------------------END

	// Inserting a 2 layer node between 2 and 3 would mean 2 and 1 is previous respectively.
	// Another insertion happening concurrently between 3 and END would have 3 and 1 as previous respectively
	// These could both succeed in linking, then it would be a race to link with node # 1 at second layer.

	// The question is whether this is a practical issue. It is a fact that whatever inserter succeeds at linking,
	// the links will move FORWARD. If doing it this way means additional synchronization may be avoided, it could 
	// really be the better way of doing it.. 

	// What about 3 layers:

	// 1------------------------------------------------------------END
	// 1------------------2-----------------------------------------END
	// 1------------------2--------------------3--------------------END

	// Allowing an inserter to continue attempting linking after failing a second layer link would mean we could end up 
	// with the following scenario:

	// 1------------------2-----------------------------------------4------------------->5>-------------------END
	// 1----------------------------------------------------------->4>-------------------5--------------------END
	// 1------------------2--------------------3--------------------4--------------------5--------------------END

	// if 4 & 5 were inserting at the same time, 4 could end up missing link at second layer, while succeeding link
	// at 3rd layer. Would this be an issue? In a forward searching scenario, 4 would be encountered first, and followed
	// by END. This would cause searcher to descend to second layer, which would also be END. In the end, search would
	// eventually be resolved at the bottom layer.

	// If we instead disallow more linking after failing one we would end up with:

	// 1------------------2-----------------------------------------*--------------------5--------------------END
	// 1------------------------------------------------------------*--------------------5--------------------END
	// 1------------------2--------------------3--------------------4--------------------5--------------------END

	// The first scenario would be a benefit if we wanted to arrive at slot between 4 & 5, since we would find 4 instantly.
	// The first might cause more sync traffic though. Also, to note, layer 2 link for node 5 would be a dead link.
	// The second scenario seems like the more correct one.

	// Now, is it possible that a link ends up leading backwards? Not from inserts.. I think.




		//for (;;) {
		//	if (!m_comparator(next.to_node()->m_kv.first, node->m_kv.first)) {
		//		// Next fails comparator test, try link after current

		//		if () {
		//			// Link succeeded, now recheck to make sure we're not in deletion territory.


		//			// Only worrying about insertions for now. It might be that index validity check can be
		//			// skipped..
		//			if (/*cpq_detail::valid_index(stepped, recentRefs, m_head.m_next.m_value)*/true){
		//				// We've linked at a valid index, all done!
		//				  return;
		//			}
		//		}
		//		else {
		//			// Not sure reattempting would be feasible with multi layer setup. Skip for now.

		//			// This'll be another inserter linking after current. Need to reload next
		//			//next = current.to_node()->m_next.load();

		//			break;
		//		}
		//	}
		//}

//template<class Key, class Value, class Compare, class Allocator>
//inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
//{
//	node_view headNext(m_head.m_next[0].inc_version());
//	node_view at(headNext);
//	node_view next(at);
//
//	bool result(false);
//
//	for (;;) {
//		if (at.to_node() == &m_head) {
//			break;
//		}
//
//		next = at.to_node()->m_next[0].inc_version();
//
//		if (next.m_version == 1) {
//			out = at.to_node();
//			result = true;
//			break;
//		}
//
//		at = next;
//	}
//
//	m_head.m_next[0].m_value.compare_exchange_strong(headNext.m_value, (std::uintptr_t)next.to_node());
//
//
//	return result;
//}









		// The best option would be if the only synchronization was between the successful deleter and
		// the faulty inserter(s). The issue is, the sync point would need to be established before deleter
		// loads the next node, which will cause any deleters atteemting to delete front to have to sync amongst themselves..
		// I suppose minimal damage would be to hold a cache padded variable and pre-load to minimize cacheline transfers..

		// Another option might be that the deleter might be able to scan forward, and, in case original next was replaced,
		// reinsert all nodes up until original next is encounterd. All the nodes until that point *should* be invisible to
		// other deleters (existing in a stale section of the list). 


		// Perhaps try both. Upper layers will prove a real challenge.

		// What about upper layers?

		// Inserters does not need to worry about other inserters.. 
		// But. 
		// Say a node is deleted, once it is deleted, any links up until < node height made from head will have to be re-linked to not have dangling
		// links. 
		// Once a de-link has happened, can it be guaranteed that an inserter will not corrupt?

		// In the case of 

		// HEAD------------------1--------------------2--------------------END
		// HEAD------------------1--------------------2--------------------END

		// A deleter begins by unlinking 1, layer 2, stalls
		// An inserter that wants to insert 0.5, effectively supplanting 1 loads the 
		// forward links 
		// -> 2
		// -> 1
		// Then begins insertion at bottom layer. If the inserter succeeds, the de-linking procedeure will effectively 
		// fail. However, since only the bottom layer really matters, this will effectively only mean that the number of links are
		// reduced to the now-second place node.

		// --------!!!------------------
		// SO one thing to remember is that if an entry is 'tagged' to be removed, it cannot be guaranteed to be at the front position
		// when cas is attempted.

		// So that could get very messy.

		// (Probably try forward scan method first)
		// --------!!!------------------



		// !!------------------- FAULTY REASONING -------------------------------------!!
		// What about this:

		// HEAD----------------------------------------------------------3--------------------END
		// HEAD-------------------------------------2--------------------3--------------------END
		// HEAD------------------1------------------2--------------------3--------------------END

		// An insertion happening at 0.5 would prepare an insertion set of 
		// HEAD
		// HEAD
		// HEAD
		// ,
		// -->3
		// -->2
		// -->1

		// Succeeding in linking bottom layer, stall
		// Deleter de-linking bottom layer
		// Inserter resumes, succeeds linking layer 2, 3.. = Dangling links.
		// Uh headache.. 

		// Should deleter prevent the faulty linking? --Somehow?
		// Or may  inserter?

		// The thing is, if the faulty links are left in there, they will potentially bury themselves. They will *not* auto-correct
		// like failure to fully link


		// One thing to note is that only the HEAD node is of issue. We want to avoid implementing a synchronization method that taxes (most?) other cases.
		// Both inserters and deleters can be distinctly aware when they're dealing with HEAD node vs not.
		// Since inserters deal with HEAD much more seldomly than does deleters, the burden or synchronization really should fall on them, if possible.

		// Probably do something with versioning and height...

		// Imagining the following:
		// For each layer at HEAD, of that height which front is assigned to, ensure that version is increased,
		// so that, even if front item has failed to link upper layers, it will properly... Waait. 

		// !!------------------------------------------------------------------------!!


		// There are additional concerns with concurrent linking and de-linking though. - I'm sure.
		// What about:
		//
		// 1------------------------------------------------------------END
		// 1------------------2--------------------3--------------------END
		// Inserter attempts insert 0.5
		// Inserter loads 1:layer 2, layer 1
		// Delter begins delinking 1:layer 2
		// Inserter succeeds linking 0.5:layer1
		// Deleter fails delinking 1:layer 1

		// Another sequence:
		// Inserter attempts insert 0.5
		// Inserter succeeds linking 0.5:layer 1
		// Deleter succeeds delinking 1:layer 1
		// Inserter fails linking 0.5:layer 2
		// ->
		// ----------------------------------------------------------------------------------END
		// 0.5------------------1------------------2--------------------3--------------------END

		// Hmm. So the idea why concurrent linking and de-linking COULD work is that de-linking happens from top, and linking happens
		// from bottom. We want this to cause a consistent collision each time. It might be that we have to sequence the loads of the links properly
		// , like, perhaps inserters need to load from top-bottom and deleters bottom-top ? Maybe.

		// If an inserter loads top -> bottom that means bottom is most up-to-date... ? 
		// If a deleter loads bottom -> top top is most up to date..... Since deleters begin at top..

		// Hmm. Might just have to experiment with that. Ah and also there's the versioning, but that'll probably just be for HEAD





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





		// Right, deletion territory was it.. :O

		// So:

		// HEAD----------------------------------------------------------3--------------------END
		// HEAD-------------------------------------2--------------------3--------------------END
		// HEAD------------------1------------------2--------------------3--------------------END

		// Inserter wants to insert 3.5, loads 3, stalls
		// Deleter deletes 1, 2, 3
		// Inserter resumes, links to 3

		// What then.. ? Ugh. Tough nut without enforcing sync at node level..
		// So if we were at base layer we could count our location, and check that not this many had been dequeued.... .?

		// Load head-> perform insert -> check how many steps since load and probe forward to see if node is encountered? :O

		// HEAD----------------------------------------------------------3--------------------END
		// HEAD-------------------------------------2--------------------3--------------------END
		// HEAD------------------1------------------2--------------------3--------------------END

		// Perhaps keep a separate variable that a successful deleter can flag, then re-check m_next to make sure it hasn't changed. Doing this a->b from
		// deleter perspective and b->a from inserter should guarantee ONE of them may detect the discrepancy. 

		// This might also work with tagging bottom layer m_next. Will have to experiment with that. Size, cache efficiency vs potential cacheline traffic


		// If we know it has been deleted, we may just continue. Oh wait.. 




//template<class Key, class Value, class Compare, class Allocator>
//inline bool concurrent_priority_queue<Key, Value, Compare, Allocator>::try_pop(typename concurrent_priority_queue<Key, Value, Compare, Allocator>::node_type*& out)
//{
//	node_view_set frontSet;
//	node_view_set replacementSet;
//
//	do {
//		frontSet[0] = m_head.m_next[0].load();
//
//		node_type* const frontNode(frontSet[0]);
//		if (frontNode == &m_head) {
//			return false;
//		}
//
//
//		// Will need to prevent linkage to stale node in upper layer by provoking cas failure in inserters.
//		// Hmm could get tricky. Me tired
//
//		// So the natural way to do this is to enforce linkage of all next links in front node at HEAD. 
//		// Probably need to load front-> reload all HEAD[height] alt. load entire HEAD[next] stack
//
//		// To begin with, probably just load entire head stack up front when doing deletion.
//
//
//
//		const std::uint8_t frontHeight(frontNode->m_height);
//
//		std::uint8_t linked(1);
//		for (; linked < frontHeight; ++linked) {
//			node_view view(m_head.m_next[linked].load());
//			if (view != frontNode) {
//				break;
//			}
//			frontSet[linked] = view;
//		}
//
//		for (std::uint8_t i = 0; i < linked; ++i) {
//			replacementSet[i] = frontNode->m_next[i].load();
//			replacementSet[i].set_version(frontSet[i].get_version() + 1);
//		}
//
//		for (std::uint8_t i = 1; i < linked; ++i) {
//			const std::uint8_t layer(linked - i);
//			m_head.m_next[layer].m_value.compare_exchange_strong(frontSet[layer].m_value, replacementSet[i].m_value, std::memory_order_seq_cst, std::memory_order_relaxed);
//		}
//
//	} while (!m_head.m_next[0].m_value.compare_exchange_weak(frontSet[0].m_value, replacementSet[0].m_value, std::memory_order_seq_cst, std::memory_order_relaxed));
//
//	node_type* const claimed(frontSet[0]);
//
//#if defined (_DEBUG)
//	claimed->m_removed = 1;
//#endif
//
//	unlink_successors(claimed, (node_type*)replacementSet[0]);
//
//	out = claimed;
//
//
//	return true;
//}