#pragma once




#include <atomic>
#include <cstdint>
#include <vector>
#include <cassert>
#include <thread>

namespace gdul {

struct control {
	enum type : std::uint32_t {
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
struct index_composite
{
	index_composite() = default;
	index_composite(std::uint64_t value)
		: m_value(value) {}

	union {
		std::uint64_t m_value;
		struct {
			std::uint16_t m_insertionIndex;
			std::uint16_t m_deleteionIndex;
			std::uint16_t m_level;

			std::uint16_t padding;
		};
	};
};

constexpr std::uint16_t pow2(std::uint16_t of) {
	std::uint16_t accum(1);

	for (std::uint16_t i = 0; i < of; ++i) {
		accum *= 2;
	}

	return accum;
}

class cpq
{
public:
	cpq()
		: m_items(64)
		, m_indices(0)
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

	cpq_item make_faux_item() const {
		cpq_item item;
		item.m_item = std::numeric_limits<std::uint32_t>::max();
		item.m_control = control::evaluate;

		return item;
	}

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

	std::uint16_t level_capacity(std::uint16_t level) const {
		return pow2(level);
	}

	std::uint16_t claim_insertion_index() {
		index_composite indices(m_indices.load());
		index_composite replacement;

		do {
			const std::uint16_t levelCapacity(level_capacity(indices.m_level));

			if (indices.m_insertionIndex == (indices.m_deleteionIndex + levelCapacity)) {
				replacement.m_insertionIndex = 1;
				replacement.m_deleteionIndex = 0;
				replacement.m_level = indices.m_level + 1;
			}
			else {
				replacement.m_insertionIndex = indices.m_insertionIndex + 1;
			}

		} while (!m_indices.compare_exchange_weak(indices, replacement));



		return pow2(replacement.m_level) + replacement.m_insertionIndex - replacement.m_deleteionIndex - 1;
	}

	bool try_claim_deletion_index(std::uint16_t& outIndex) {

	}

	void push(std::uint32_t key) {
		const std::uint16_t index(claim_insertion_index());

		cpq_item insert(key);
		insert.m_control = control::swap_up;

		help_complete_swap_up(index, insert);
	}
	bool try_pop(std::uint32_t& outKey)
	{
		std::uint16_t index;
		if (!try_claim_deletion_index(index)) {
			return false;
		}

		const cpq_item back(m_items[index].exchange(make_faux_item()));

		if (index != 0) {
			cpq_item top(m_items[0].load());

			do {
				if (top.m_control) {
					std::this_thread::yield();
					top = m_items[0].load();
					continue;
				}
			} while (!m_items[0].compare_exchange_strong(top, back));

			help_complete_swap_down(0, cpq_item());
		}

		return false;
	}

	std::vector<std::atomic<cpq_item>> m_items;

	std::atomic<index_composite> m_indices;
};
}