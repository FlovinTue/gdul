#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <algorithm>
#include <thread>
#include <gdul/memory/concurrent_scratch_pool.h>
#include <gdul/concurrent_skip_list/concurrent_skip_list_base.h>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

#pragma region detail

namespace gdul {
namespace csl_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
class alignas(std::hardware_destructive_interference_size) concurrent_skip_list_impl;

}
#pragma endregion

template <class Key, class Value, csl_detail::size_type ExpectedListSize = 512, class Compare = std::less<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_skip_list : public csl_detail::concurrent_skip_list_impl<Key, Value, csl_detail::to_tower_height(ExpectedListSize), Compare, Allocator>
{
public:
	using csl_detail::concurrent_skip_list_impl<Key, Value, csl_detail::to_tower_height(ExpectedListSize), Compare, Allocator>::concurrent_skip_list_impl;
};

namespace csl_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
class alignas(std::hardware_destructive_interference_size) concurrent_skip_list_impl : public concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>
{
public:
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::key_type;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::value_type;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::comparator_type;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::iterator;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::const_iterator;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::size_type;
	using typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::node_type;
	
	using allocator_type = Allocator;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_skip_list_impl();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="alloc">Allocator passed to node pool</param>
	concurrent_skip_list_impl(allocator_type);

	/// <summary>
	/// Destructor
	/// </summary>
	~concurrent_skip_list_impl() noexcept;

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted</param>
	std::pair<iterator, bool> insert(const std::pair<Key, Value>& in);

	/// <summary>
	/// Insert an item
	/// </summary>
	/// <param name="in">Item to be inserted using move semantics</param>
	std::pair<iterator, bool> insert(std::pair<Key, Value>&& in);

	/// <summary>
	/// Reset to initial state
	/// Assumes no concurrent inserts and an empty list
	/// </summary>
	void unsafe_reset();

	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::begin;
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::end;
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::find;

private:
	using node_set = node_type * [LinkTowerHeight];
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::m_head;

	template <class In>
	std::pair<iterator, bool> insert_internal(In&& in);
	std::pair<iterator, bool> try_insert(node_type* node);

	bool find_slot(node_set& atSet, node_set& nextSet, const node_type* node) const;

	concurrent_scratch_pool<node_type, Allocator> m_pool;
};
template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::concurrent_skip_list_impl()
	: concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>(allocator_type())
{
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::concurrent_skip_list_impl(typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::allocator_type alloc)
	: concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::concurrent_skip_list_base()
	, m_pool(csl_detail::to_expected_list_size(LinkTowerHeight), csl_detail::to_expected_list_size(LinkTowerHeight) / std::thread::hardware_concurrency(), alloc)
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::~concurrent_skip_list_impl()
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline std::pair<typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert(typename std::pair<Key, Value>&& in)
{
	return insert_internal(std::move(in));
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline std::pair<typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert(const typename std::pair<Key, Value>& in)
{
	return insert_internal(in);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
template<class In>
inline std::pair<typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert_internal(In&& in)
{
	node_type* const n = m_pool.get();
	n->m_kv = std::forward<In>(in);
	n->m_height = random_height<>(LinkTowerHeight);

	std::pair<iterator, bool> result;

	do {
		result = try_insert(n);
	} while (result.first->first != n->m_kv.first);

	return result;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline void concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::unsafe_reset()
{
	assert(this->empty() && "Bad call to unsafe_reset, there are still items in the list");
	this->m_pool.unsafe_reset();

	std::for_each(std::begin(m_head.m_links), std::end(m_head.m_links), [this](std::atomic<node_type*>& link) {link.store(&m_head, std::memory_order_relaxed); });
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline bool concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::find_slot(node_set& atSet, node_set& nextSet, const node_type* node) const
{
	constexpr comparator_type comparator;

	const key_type key(node->m_kv.first);

	for (std::uint8_t i = 0; i < LinkTowerHeight; ++i) {
		const std::uint8_t layer(LinkTowerHeight - i - 1);

		for (;;) {
			nextSet[layer] = atSet[layer]->m_links[layer].load(std::memory_order_relaxed);

			const key_type nextKey(nextSet[layer]->m_kv.first);

			if (!comparator(nextKey, key)) {

				if (!comparator(key, nextKey)) {
					atSet[0] = nextSet[layer];
					return false;
				}

				break;
			}

			atSet[layer] = nextSet[layer];
		}

		if (layer) {
			atSet[layer - 1] = atSet[layer];
		}
	}

	return true;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline std::pair<typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::try_insert(typename concurrent_skip_list_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::node_type* node)
{
	node_set atSet{};
	node_set nextSet{};

	atSet[LinkTowerHeight - 1] = &m_head;

	if (!find_slot(atSet, nextSet, node)) {
		return std::make_pair(iterator(atSet[0]), false);
	}

	const std::uint8_t height(node->m_height);

	for (std::uint8_t i = 0; i < height; ++i) {
		node->m_links[i].store(nextSet[i], std::memory_order_relaxed);
	}

	if (!atSet[0]->m_links[0].compare_exchange_strong(nextSet[0], node, std::memory_order_relaxed)) {
		return std::make_pair(this->end(), false);
	}

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		atSet[layer]->m_links[layer].compare_exchange_strong(nextSet[layer], node, std::memory_order_relaxed);
	}

	return std::make_pair(iterator(node), true);
}
}

}
#pragma warning(pop)