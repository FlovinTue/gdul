//Copyright(c) 2020 Flovin Michaelsen
//
//Permission is hereby granted, free of charge, to any person obtining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <algorithm>
#include <thread>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/containers/concurrent_skip_list_base.h>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

#pragma region detail

namespace gdul {
namespace csl_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
class alignas(std::hardware_destructive_interference_size) concurrent_map_impl;

}
#pragma endregion


/// <summary>
/// A concurrency safe lock-free map based on skip list design
/// </summary>
/// <typeparam name="Key">Key type</typeparam>
/// <typeparam name="Value">Value type</typeparam>
/// <typeparam name="ExpectedMapSize">How many items is expected to be in the map at any one time. Hint to determine node tower height</typeparam>
/// <typeparam name="Allocator">Allocator passed to the node pool</typeparam>
/// <typeparam name="Compare">Comparator to decide node ordering</typeparam>
template <class Key, class Value, csl_detail::size_type ExpectedMapSize = 512, class Compare = std::less<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_map : public csl_detail::concurrent_map_impl<Key, Value, csl_detail::to_tower_height(ExpectedMapSize), Compare, Allocator>
{
public:
	using csl_detail::concurrent_map_impl<Key, Value, csl_detail::to_tower_height(ExpectedMapSize), Compare, Allocator>::concurrent_map_impl;
};

namespace csl_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
class alignas(std::hardware_destructive_interference_size) concurrent_map_impl : public concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>
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
	concurrent_map_impl();

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="alloc">Allocator passed to node pool</param>
	concurrent_map_impl(allocator_type);

	/// <summary>
	/// Destructor
	/// </summary>
	~concurrent_map_impl() noexcept;

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

	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::begin;
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::end;
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::find;

	/// <summary>
	/// Reset to initial state
	/// Assumes no concurrent inserts and an empty list
	/// </summary>
	void unsafe_reset();

	/// <summary>
	/// Removes items
	/// Not concurrency safe
	/// </summary>
	void unsafe_clear();

private:
	using node_view = typename node_type::node_view;
	using node_view_set = node_view [LinkTowerHeight];
	using concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::m_head;

	template <class In>
	std::pair<iterator, bool> insert_internal(In&& in);
	std::pair<iterator, bool> try_insert(node_type* node);

	bool find_slot(node_view_set& atSet, node_view_set& nextSet, const node_type* node) const;

	concurrent_guard_pool<node_type, Allocator> m_pool;
};
template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::concurrent_map_impl()
	: concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>(allocator_type())
{
}
template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::concurrent_map_impl(typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::allocator_type alloc)
	: concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::concurrent_skip_list_base()
	, m_pool((typename decltype(m_pool)::size_type)csl_detail::to_expected_list_size(LinkTowerHeight), (typename decltype(m_pool)::size_type)csl_detail::to_expected_list_size(LinkTowerHeight) / std::thread::hardware_concurrency(), alloc)
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::~concurrent_map_impl()
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline std::pair<typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert(typename std::pair<Key, Value>&& in)
{
	return insert_internal(std::move(in));
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline std::pair<typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert(const typename std::pair<Key, Value>& in)
{
	return insert_internal(in);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
template<class In>
inline std::pair<typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::insert_internal(In&& in)
{
	node_type* const n = m_pool.get();
	n->m_kv = std::forward<In>(in);
	n->m_height = random_height<>(LinkTowerHeight);

	std::pair<iterator, bool> result;

	do {
		result = try_insert(n);
	} while (result.first->first != n->m_kv.first);

	if (!result.second) {
		m_pool.recycle(n);
	}

	return result;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline void concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::unsafe_reset()
{
	assert(this->empty() && "Bad call to unsafe_reset, there are still items in the list");
	this->m_pool.unsafe_reset();

	std::for_each(std::begin(m_head.m_linkViews), std::end(m_head.m_linkViews), [this](std::atomic<node_view>& link) {link.store(&m_head, std::memory_order_relaxed); });
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline void concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::unsafe_clear()
{
	node_type* n(m_head.m_linkViews[0].load(std::memory_order_relaxed));

	while (!this->at_end(n)) {
		node_type* const next(n->m_linkViews[0].load(std::memory_order_relaxed));

		m_pool.recycle(n);

		n = next;
	}

	std::for_each(std::begin(m_head.m_linkViews), std::end(m_head.m_linkViews), [this](std::atomic<node_view>& link) {link.store(&m_head, std::memory_order_relaxed); });
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare, class Allocator>
inline bool concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::find_slot(node_view_set& atSet, node_view_set& nextSet, const node_type* node) const
{
	constexpr comparator_type comparator;

	const key_type key(node->m_kv.first);

	for (std::uint8_t i = 0; i < LinkTowerHeight; ++i) {
		const std::uint8_t layer(LinkTowerHeight - i - 1);

		for (;;) {
			nextSet[layer] = atSet[layer].m_node->m_linkViews[layer].load(std::memory_order_relaxed);

			const key_type nextKey(nextSet[layer].m_node->m_kv.first);

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
inline std::pair<typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::iterator, bool> concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::try_insert(typename concurrent_map_impl<Key, Value, LinkTowerHeight, Compare, Allocator>::node_type* node)
{
	node_view_set atSet{};
	node_view_set nextSet{};

	atSet[LinkTowerHeight - 1].m_node = &m_head;

	if (!find_slot(atSet, nextSet, node)) {
		return std::make_pair(iterator(atSet[0].m_node), false);
	}

	const std::uint8_t height(node->m_height);

	for (std::uint8_t i = 0; i < height; ++i) {
		node->m_linkViews[i].store(nextSet[i], std::memory_order_relaxed);
	}

	if (!atSet[0].m_node->m_linkViews[0].compare_exchange_strong(nextSet[0], node, std::memory_order_relaxed)) {
		return std::make_pair(this->end(), false);
	}

	for (std::uint8_t layer = 1; layer < height; ++layer) {
		atSet[layer].m_node->m_linkViews[layer].compare_exchange_strong(nextSet[layer], node, std::memory_order_relaxed);
	}

	return std::make_pair(iterator(node), true);
}
}

}
#pragma warning(pop)
