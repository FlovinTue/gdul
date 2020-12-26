#pragma once

#include <atomic>
#include <stdint.h>
#include <memory>
#include <algorithm>
#include <thread>

#pragma warning(push)
// Alignment padding
#pragma warning(disable : 4324)

#pragma region detail

namespace gdul {
namespace csl_detail {

using size_type = std::size_t;

constexpr size_type log2_ceil(size_type value)
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
constexpr std::uint8_t to_tower_height(size_type expectedListSize)
{
	const std::uint8_t naturalHeight((std::uint8_t)log2_ceil(expectedListSize));
	const std::uint8_t halfHeight(naturalHeight / 2 + bool(naturalHeight % 2));

	return halfHeight;
}
constexpr size_type to_expected_list_size(std::uint8_t towerHeight)
{
	size_type base(1);

	for (std::uint8_t i = 0; i < towerHeight * 2; ++i) {
		base *= 2;
	}

	return base;
}

template <class Key, class Value, std::uint8_t LinkTowerHeight>
struct node;
template <class Node>
struct const_forward_iterator;
template <class Node>
struct forward_iterator;

}
#pragma endregion

namespace csl_detail {

template <class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
class alignas(std::hardware_destructive_interference_size) concurrent_skip_list_base
{
public:
	using key_type = Key;
	using value_type = Value;
	using compare_type = Compare;
	using node_type = node<Key, Value, LinkTowerHeight>;
	using size_type = typename size_type;
	using comparator_type = Compare;
	using iterator = csl_detail::forward_iterator<node_type>;
	using const_iterator = csl_detail::const_forward_iterator<const node_type>;

	/// <summary>
	/// Constructor
	/// </summary>
	concurrent_skip_list_base();

	/// <summary>
	/// Destructor
	/// </summary>
	~concurrent_skip_list_base() noexcept;

	/// <summary>
	/// Query for items
	/// </summary>
	/// <returns>True if no items exists in list</returns>
	bool empty() const noexcept;

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>At node with matching key on success. end on failure</returns>
	iterator find(key_type k);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>At node with matching key on success. end on failure</returns>
	const_iterator find(key_type k) const;

	/// <summary>
	/// Iterator to first element
	/// </summary>
	/// <returns>Iterator to first element if any present. end if empty</returns>
	iterator begin();

	/// <summary>
	/// Iterator to first element
	/// </summary>
	/// <returns>Iterator to first element if any present. end if empty</returns>
	const_iterator begin() const;

	/// <summary>
	/// Iterator to end
	/// </summary>
	/// <returns>Iterator to end element</returns>
	iterator end();

	/// <summary>
	/// Iterator to end
	/// </summary>
	/// <returns>Iterator to end element</returns>
	const_iterator end() const;
protected:

	template <class Iterator>
	static Iterator find_internal(Iterator head, key_type k);

	bool at_end(const node_type* n) const;

	// Also end
	node_type m_head;
};
template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::concurrent_skip_list_base()
	: m_head()
{
	m_head.m_kv.first = std::numeric_limits<key_type>::max();

	for (size_type i = 0; i < LinkTowerHeight; ++i) {
		m_head.m_next[i].store(&m_head, std::memory_order_relaxed);
	}
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::~concurrent_skip_list_base()
{
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
template<class Iterator>
inline Iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::find_internal(Iterator head, typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::key_type k)
{
	constexpr comparator_type comparator;

	Iterator at(head);

	for (std::uint8_t i = 0; i < LinkTowerHeight; ++i) {
		const std::uint8_t layer(LinkTowerHeight - i - 1);

		for (;;) {
			Iterator next{ at.m_at->m_next[layer].load(std::memory_order_relaxed) };

			const key_type atKey((*next).first);

			if (k == atKey) {
				return next;
			}

			if (!comparator(atKey, k)) {
				break;
			}

			at = next;
		}
	}

	return head;
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::find(typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::key_type k)
{
	return find_internal<iterator>(iterator(&m_head), k);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::const_iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::find(typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::key_type k) const
{
	return find_internal<const_iterator>(const_iterator(&m_head), k);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::begin()
{
	return { m_head.m_next[0].load(std::memory_order_relaxed) };
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::const_iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::begin() const
{
	return { m_head.m_next[0].load(std::memory_order_relaxed) };
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::end()
{
	return { &m_head };
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline typename concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::const_iterator concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::end() const
{
	return const_iterator(&m_head);
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline bool concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::empty() const noexcept
{
	return at_end(m_head.m_next[0].load(std::memory_order_relaxed));
}

template<class Key, class Value, std::uint8_t LinkTowerHeight, class Compare>
inline bool concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::at_end(const concurrent_skip_list_base<Key, Value, LinkTowerHeight, Compare>::node_type* n) const
{
	return n == &m_head;
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

template <class Dummy = void>
std::uint8_t random_height(std::uint8_t maxHeight)
{
	std::uint8_t result(1);

	for (std::uint8_t i = 0; i < maxHeight - 1; ++i, ++result) {
		if ((t_rng() % 4u) != 0) {
			break;
		}
	}

	return result;
}

template <class Key, class Value, std::uint8_t LinkTowerHeight>
struct node
{
	using key_type = Key;
	using value_type = Value;

	union node_view
	{
		using node_type = node;

		node_view() = default;
		node_view(node_type* n)
			: m_value((std::uintptr_t)(n))
		{
		}
		node_view(node_type* n, std::uint32_t version)
			:node_view(n)
		{
			set_version(version);
		}

		operator node_type* ()
		{
			return (node_type*)(m_value & Pointer_Mask);
		}
		operator const node_type* () const
		{
			return (const node_type*)(m_value & Pointer_Mask);
		}

		operator std::uintptr_t() = delete;

		bool operator ==(node_view other) const
		{
			return operator const node_type * () == other.operator const node_type * ();
		}
		bool operator !=(node_view other) const
		{
			return !operator==(other);
		}
		operator bool() const
		{
			return operator const node_type * ();
		}
		bool has_version() const
		{
			return m_value & Version_Mask;
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

		node_type* m_node;
		std::uintptr_t m_value;
	};

	node() : m_kv(), m_linkViews{}, m_height(LinkTowerHeight) {}
	node(std::pair<Key, Value>&& item) : m_kv(std::move(item)), m_linkViews{}, m_height(LinkTowerHeight) {}
	node(const std::pair<Key, Value>& item) : m_kv(item), m_linkViews{}, m_height(LinkTowerHeight){}



	std::atomic<node_view> m_linkViews[LinkTowerHeight];
	std::uint8_t m_height;
	std::pair<Key, Value> m_kv;
};
template <class Node>
struct iterator_base
{
	using iterator_tag = std::forward_iterator_tag;
	using key_type = typename Node::key_type;
	using value_type = typename Node::value_type;

	iterator_base()
		: m_at(nullptr)
	{
	}

	iterator_base(const iterator_base<Node>& itr)
		: iterator_base(itr.m_at)
	{
	}

	iterator_base(Node* n)
		: m_at(n)
	{
	}

	const std::pair<key_type, value_type>& operator*() const
	{
		return m_at->m_kv;
	}

	const std::pair<key_type, value_type>* operator->() const
	{
		return &m_at->m_kv;
	}

	bool operator==(const iterator_base& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const iterator_base& other) const
	{
		return !operator==(other);
	}

	Node* m_at;
};

template <class Node>
struct forward_iterator : public iterator_base<Node>
{
	using iterator_base<Node>::iterator_base;

	forward_iterator operator++()
	{
		return { this->m_at->m_next[0].load(std::memory_order_relaxed) };
	}

	std::pair<iterator_base<Node>::key_type, iterator_base<Node>::value_type>& operator*()
	{
		return this->m_at->m_kv;
	}

	std::pair<iterator_base<Node>::key_type, iterator_base<Node>::value_type>* operator->()
	{
		return &this->m_at->m_kv;
	}
};
template <class Node>
struct const_forward_iterator : public iterator_base<Node>
{
	using iterator_base<Node>::iterator_base;

	const_forward_iterator(forward_iterator<std::remove_const_t<Node>> fi)
		: iterator_base<Node>(fi.m_at)
	{
	}

	const_forward_iterator operator++() const
	{
		return { this->m_at->m_next[0].load(std::memory_order_relaxed) };
	}
};
}

}
#pragma warning(pop)
