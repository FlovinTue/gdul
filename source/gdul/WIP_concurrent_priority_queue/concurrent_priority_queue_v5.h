#pragma once

#include <vector>
#include <atomic>
#include <thread>

#pragma warning(disable : 4201)



// Problems:
// #1 conflicts of inserts / dequeues meeting in a branch
// #2 claiming last spot & decrementing size in face of inserts ( Perhaps use an (atomic) end pointer.. With versioning?) (Or maybe a size variable, with versioning?.. load -> cas decr / load -> store -> cas incr?)
// #3 (more trivial) growing array? 
// #4 deadlock possible if flagging right - > stall -> other flag left ?? 



// To expand on #1:
// In the event of a conflict, a consumer will want to swap a parent downward
// and a producer will want to swap a child upward. In the end, these are the same operation.

// Something to take note of is when one child is flagged, but not the lower of the children.
// then the conflicting producer will not necessarily have a clear cut path upward. (it might 
// be that the consumer will need to swap up the sibling before the next swap can happen..)

// Another scenario is when a consumer meets TWO producers, one at each child.. 

// What will be needed is operation containers, that may be used in a helping mechanism
// .. Something like:
// struct repair_op
// {
//	node_type* m_src;
//	node_type* m_target
// }
// and then maybe use a reference to that as flag...

// So, more detailed: We need the repair_op to be able to proceed using atomic, generalized steps.
// when either a consumer or a producer encounter a foreign repair_op it should be able to complete it without 
// the help of the other operator...

// The fundamental steps of a shift-down are:
// * Flag right, left
// * Compare right, left
// * Unflag higher key child
// * Compare parent, child
// * Based on comparison result, swap or not
// * Unflag parent
// * Either repeat or release child

// Hmm. Need to figure out how to handle transition at end of operation



namespace gdul {

namespace cpq_detail {
using size_type = std::size_t;

template <class Key, class Value>
union debug_viewable {
	const std::pair<Key, Value>* m_debugView;

	struct {
		std::atomic<uintptr_t> m_val;
	};
};

}

template <class Key, class Value>
class cpq_test
{
public:
	cpq_test()
		: m_nodes(100)
	{}

	using node_type = std::pair<Key, Value>;
	using size_type = typename cpq_detail::size_type;

	bool try_pop(std::pair<Key, Value>& out);

	void insert(std::pair<Key, Value> item);


private:

	void flag(size_type at, node_type*& out) {
		while (!try_flag(at, out))
			std::this_thread::yield();
	}

	bool try_flag(size_type at, node_type*& out) {
		const uintptr_t v(m_nodes[at].m_val.fetch_or(1));
		if (v & uintptr_t(1))
			return false;

		out = (node_type*)v;

		return true;
	}

	void clear_flag(size_type at) {
		m_nodes[at].m_val.fetch_and(~uintptr_t(1));
	}


	std::vector<cpq_detail::debug_viewable<Key, Value>> m_nodes;

	std::atomic_uint m_size;
};
template<class Key, class Value>
inline bool cpq_test<Key, Value>::try_pop(std::pair<Key, Value>& out)
{
	const size_type size(m_size.fetch_sub(1));

	if (std::numeric_limits<size_type>::max() / 2 < size)
		return false;

	node_type* const replacement((node_type*)m_nodes[size - 1].m_val.load());

	node_type* probe(nullptr);
	flag(0, probe);

	out = *probe;

	*probe = *replacement;

	size_type next(0);
	size_type left(1);
	size_type right(2);

	while (next < size) {
		const size_type last(next);

		node_type* nextNode(probe);

		if (right < size) {
			node_type* rightNode(nullptr);
			flag(right, rightNode);
			if (rightNode->first < nextNode->first) {
				nextNode = rightNode;
				next = right;
			}
			else {
				clear_flag(right);
			}
		}

		if (left < size) {
			node_type* leftNode(nullptr);
			flag(left, leftNode);
			if (leftNode->first < nextNode->first) {
				nextNode = leftNode;
				next = left;

				clear_flag(right);
			}
			else {
				clear_flag(left);
			}
		}

		if (nextNode == probe) {
			clear_flag(last);
			break;
		}

		std::swap(*probe, *nextNode);

		clear_flag(last);

		probe = nextNode;
		left = next * 2 + 1;
		right = next * 2 + 2;
	}

	return true;
}
template<class Key, class Value>
inline void cpq_test<Key, Value>::insert(std::pair<Key, Value> item)
{
	size_type index(m_size++);
	size_type parent((index - 1) / 2);


	node_type* toInsert(new node_type(item));

	m_nodes[index].m_val.store((uintptr_t)toInsert);

	for (;;)
	{
		if (index == 0)
			break;

		node_type* parentNode(nullptr);

		flag(parent, parentNode);

		if (parentNode->first < item.first) {
			clear_flag(parent);
			break;
		}

		node_type* node((node_type*)m_nodes[index].m_val.load());

		std::swap(*parentNode, *node);

		clear_flag(parent);

		index = parent;
		parent = (index - 1) / 2;
	}
}
}