#pragma once


#include <xhash>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/thread_local_member/thread_local_member.h>
#include <gdul/atomic_128/atomic_128.h>

namespace gdul {

namespace chm_detail {
using size_type = std::size_t;

template <class Bucket>
class forward_iterator;
template <class Bucket>
class const_forward_iterator;

template <class Key, class Value>
struct bucket;

}

template <class Key, class Value, class Hash = std::hash<Key>, class Allocator = std::allocator<std::uint8_t>>
class concurrent_hash_map
{
public:
	using size_type = typename chm_detail::size_type;
	using key_type = Key;
	using value_type = Value;
	using hash_type = Hash;
	using allocator_type = Allocator;
	using iterator = void;
	using const_iterator = void;
	using bucket_type = int;

	concurrent_hash_map();
	concurrent_hash_map(Allocator);

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
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>At bucket with matching key on success. end on failure</returns>
	iterator find(key_type k);

	/// <summary>
	/// Search for key
	/// </summary>
	/// <param name="k">Search key</param>
	/// <returns>At bucket with matching key on success. end on failure</returns>
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

private:
	concurrent_guard_pool<value_type, allocator_type> m_pool;
	atomic_shared_ptr<bucket_type[]> m_items;
	tlm<shared_ptr<bucket_type[]>> t_items;
	allocator_type m_allocator;
};
namespace chm_detail {
template <class Key, class Value>
struct bucket
{
	struct items
	{
		std::uint64_t hash = 0;
		std::pair<Key, Value>* kv = nullptr;
	};
	atomic_128<items> m_storage;
};


template <class Bucket>
struct iterator_base
{
	using iterator_tag = std::forward_iterator_tag;
	using key_type = typename Bucket::key_type;
	using value_type = typename Bucket::value_type;

	iterator_base()
		: m_at(nullptr)
	{
	}

	iterator_base(std::pair<raw_ptr<Bucket[]>, size_type> at)
		: m_at(at)
	{
	}

	const value_type& operator*() const
	{
		return *operator->();
	}

	const value_type* operator->() const
	{
		return m_at.first[m_at->second].m_value;
	}

	bool operator==(const iterator_base& other) const
	{
		return other.m_at == m_at;
	}

	bool operator!=(const iterator_base& other) const
	{
		return !operator==(other);
	}

	std::pair<raw_ptr<Bucket[]>, size_type> m_at;
};

template <class Bucket>
struct forward_iterator : public iterator_base<Bucket>
{
	using iterator_base<Bucket>::iterator_base;

	forward_iterator operator++()
	{
		
	}

	std::pair<iterator_base<Bucket>::key_type, iterator_base<Bucket>::value_type>& operator*()
	{
		return this->m_at->m_kv;
	}

	std::pair<iterator_base<Bucket>::key_type, iterator_base<Bucket>::value_type>* operator->()
	{
		return &this->m_at->;
	}
};
template <class Bucket>
struct const_forward_iterator : public iterator_base<Bucket>
{
	using iterator_base<Bucket>::iterator_base;

	const_forward_iterator(forward_iterator<std::remove_const_t<Bucket>> fi)
		: iterator_base<Bucket>(fi.m_at)
	{
	}

	const_forward_iterator operator++() const
	{
		return const_forward_iterator();
	}
};
template <class Key, class Value>
struct bucket
{

};
}
}