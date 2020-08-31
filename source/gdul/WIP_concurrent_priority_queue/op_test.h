#pragma once




#include <atomic>
#include <cstdint>
#include <vector>
#include <cassert>
#include <thread>

// Of note:
// There need be no 'completion-step' to an operation.
// An insert adds the item initially, and is wholly completed once the item is swapped
// into place. A pop operation will remove a back-item and then replace the front item
// with that independently. The front item will be known at this point, and the the pop operation will
// be complete when the replaced top is swapped into place..


// Something to think about. If a top swap goes down the wrong branch that means a non-smallest item
// will be swapped to top. 

namespace gdul {



struct control {
	enum type: std::uint32_t {
		tag = 1 << 0,
		evaluate = tag,
		swap_right = tag + (1 << 1),
		swap_left = tag + (1 << 2),
		swap_up = tag | swap_right | swap_left,


	};
};

struct cpq_item {
	cpq_item() = default;
	cpq_item(std::uint64_t item)
		: m_value(item) {}

	union {
		std::uint64_t m_value;
		struct {
			std::uint32_t m_item;

			union {
				control::type m_controlView;
				std::uint32_t m_control;
			};
		};
	};
};


class cpq
{
public:
	cpq()
		: m_items(64)
		, m_size(0)
	{
		std::uninitialized_fill(m_items.begin(), m_items.end(), std::numeric_limits<std::uint32_t>::max());
	}

	std::size_t parent(std::size_t of) const {
		assert(of != 0 && "Top has no parent");
		return (of - 1) / 2;
	}
	std::size_t left_child(std::size_t of) const {
		return of * 2 + 1;
	}
	std::size_t right_child(std::size_t of) const {
		return of * 2 + 2;
	}
	bool is_swapping_to(std::size_t index, cpq_item item) const {
		if (((item.m_control & control::swap_left) && (index % 2) == 1) ||
			((item.m_control & control::swap_right) && (index % 2) == 0)) {
			return true;
		}
		return false;
	}
	bool is_untagged(cpq_item item) const {
		return item.m_control & control::tag;
	}
	void setup() {
		for (std::size_t i = 0; i < 32; ++i) {
			m_items[i] = cpq_item(i + 1);
		}
		m_size = 32;
	}

	cpq_item make_faux_item() const {
		cpq_item item;
		item.m_item = std::numeric_limits<std::uint32_t>::max();
		item.m_control = control::evaluate;

		return item;
	}

	// So with the simplified idea of just tagging and laying out the operations preceeding an actual swap is hindered by the fact that
	// the two different stores that need to be done is not atomic. If one is stored to and the storer stalls, no other helper may read
	// the original value.

	// Which means we'll need a new way of doing ops. Could possibly use aggregate op structs that is filled with all info before any 
	// heap mutation occurs.

	// Doing it with aggregate op struct feels very convoluted. Like 'too many steps'. I want a shot at it being efficient (some time)
	// 

	// Could perhaps the aggregate op struct be used in the very unusual case where a helper is unable to read one of the values. (let's see
	// it'll only be one of them I think. ) 
	// Yeah I like that a lot better. And then perhaps just implement a blocking version first temporarily.

	// Another, little bit unrelated. Perhaps I should look at splitting up the queues? Utilize some sort of work stealing thing
	// What that will bring is a bit more predictable enqueue/dequeue pattern. Though, to be fair, the experimental method
	// relies on single synchronous queue..

	// So to elaborate. Owner has a node, claims other. Has full knowledge. Begins swap by .. Going up? Seems reasonable.
	// So the top will be written first, with an open state? -That depends on which is the direction.

	// So. What if 
	// Thread a claims node a, stalls
	// Thread b attempts to help -> loads node a, claims parent node b, completes swap
	// Thread a resumes, .. claims? node b in the case a has been swapped away, this may be *something*
	// then claim will complete. Cannot (?) be avoided. If we can flag it as slave this means we can unflag it. maybe.
	// Need to be able to 'fail' on claiming second node.

	// What we want, according to the best idea is that we push a node to a slot, and then attempt to complete the swap-repair operation.
	// This swap-repair operation should be totally thread agnostic and any thread should be able to take over at any time. So
	// We might need to load a, claim a, load b, load a, claim b, attempt swap. In case of failure another will have taken over.
	// Correction. (We already have 'a' claimed) load b, load a, claim b, attempt swap




	// So when doing a swap-up we know the current item is claimed. We don't need to claim the target. Rather, just load target,
	// Then (when enabling helping, reloading current to check nobody else the swap) cas target -> cas current.


	// When doing a swap-down we probably need to evaluate, then swap-up target...
	// Evaluate will still (? i think) work like envisioned. Then target will need to be tagged. Dang. No that's the two sync problem.
	// Need to find a way of doing cas the other way. Tingly sense tells me there will be a posibility of deadlocking between pushers 
	// and poppers. Otherwise it would work same way as pushing. For now, block when encountering conflicting claim -> load target, cas target
	// cas current.

	// The cases we don't have a plan for yet are: 
	// * Conflict from below when parent is swapping to sibling
	// * Conflict from below when parent is swapping up
	// * Conflict from above when child is swapping down

	// The case of conflict from below when parent is swapping to current item is fine. The objectives of swappers are
	// (pretty much) non exclusive.

	cpq_item help_complete_swap_down(std::size_t index, cpq_item item) {
		if (try_complete_swap_down(index, item)) {
			cpq_item exp(item);
			while (try_complete_swap_down(index, exp));
		}
		return item;
	}
	void help_complete_swap_up(std::size_t index, cpq_item item) {
		while (try_complete_swap_up(index, item));
	}


	bool try_complete_swap_down(std::size_t& index, cpq_item& expected) {

		std::size_t childIndex(0);
		cpq_item childItem;

		if (expected.m_control == control::evaluate) {
			
			const std::size_t leftIndex(left_child(index));
			const std::size_t rightIndex(right_child(index));

			const cpq_item leftChild(m_items[leftIndex].load());
			const cpq_item rightChild(m_items[rightIndex].load());

			cpq_item evaluated(expected);

			if (leftChild.m_item < rightChild.m_item) {
				childIndex = leftIndex;
				childItem = leftChild;
				evaluated.m_control = control::swap_left;
			}
			else {
				childIndex = rightIndex;
				childItem = rightChild;
				evaluated.m_control = control::swap_right;
			}

			if (!m_items[index].compare_exchange_strong(expected, evaluated)) {
				return false;
			}

			expected = evaluated;
		}

		if (!(childItem.m_item < expected.m_item)) {
			return false;
		}

		for (;;) {
			// Block for now in these cases...
			if (!(is_untagged(childItem) || childItem.m_control == control::swap_up)) {

				cpq_item exchange(expected);
				exchange.m_control = control::evaluate;

				// Try exchanging parent item
				if (m_items[childIndex].compare_exchange_strong(childItem, exchange)) {

					// Try exchanging old item
					if (m_items[index].compare_exchange_strong(expected, childItem) &&
						childItem.m_control == control::swap_up) {

						// In case we succeed and the parent was in a down-swap process, we'll take over the
						// responsibility of completing it's swapping
						help_complete_swap_up(index, childItem);
					}
					index = childIndex;
					expected = exchange;

					return true;
				}
				else {
					return false;
				}
			}
			else {
				std::this_thread::yield();
				childItem = m_items[childIndex].load();
			}
		}
	}
	bool try_complete_swap_up(std::size_t& index, cpq_item item) {
		if (index != 0) {
			const std::size_t parentIndex(parent(index));

			cpq_item parentItem(m_items[parentIndex].load());

			for (;;) {
				if (!is_swapping_to(index, parentItem) || is_untagged(parentIndex)) {

					// Make comparison
					if (item.m_item < parentItem.m_item) {

						// Try exchanging parent item
						if (m_items[parentIndex].compare_exchange_weak(parentItem, item)) {

							// Try exchanging old item
							if (m_items[index].compare_exchange_strong(item, parentItem) &&
								is_swapping_to(index, parentItem)) {

								// In case we succeed and the parent was in a down-swap process, we'll take over the
								// responsibility of completing it's swapping
								help_complete_swap_down(index, parentItem);
							}
							index = parentIndex;

							return true;
						}
					}
					else {
						break;
					}

				}
				else {
					std::this_thread::yield();
					parentItem = m_items[parentIndex].load();
				}
			}
		}

		cpq_item clear(item);
		clear.m_control = 0;
		m_items[index].compare_exchange_strong(item, clear);

		return false;
	}


	void push(std::uint32_t key)
	{
		std::size_t index(m_size++);

		cpq_item insert(key);
		insert.m_control = control::swap_up;

		// Insertion algorithm uninteresting for this test
		m_items[index].store(insert);


		// Probably need to consider the swap_up / evaluate mechanism as wholly ownership
		// agnostic. They just need to be completed, it does not matter who completes them, so long
		// as it is as expedient as possible.

		// here we want to (recursively) tag parent

		help_complete_swap_up(index, insert);
	}
	bool try_pop(std::uint32_t& outKey)
	{
		std::size_t size(m_size.load());

		do {
			if (!size) {
				return false;
			}

		} while (!m_size.compare_exchange_weak(size, size - 1));

		cpq_item top(m_items[0].load());
		cpq_item fauxItem(make_faux_item());

		do {
			if (top.m_control) {
				top = help_complete_swap_down(0, top);
				continue;
			}

		} while (!m_items[0].compare_exchange_weak(top, fauxItem));

		help_complete_swap_down(0, fauxItem);

		outKey = top.m_item;

		return true;
	}

	std::vector<std::atomic<cpq_item>> m_items;

	std::atomic<std::size_t> m_size;
};
}