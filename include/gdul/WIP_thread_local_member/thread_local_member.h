#pragma once

#include <vector>
#include <array>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>

namespace gdul
{
template <class T, class Allocator = std::allocator<T>>
class thread_local_member;

template <class T, class Allocator = std::allocator<T>>
using tlm = thread_local_member<T, Allocator>;

namespace thread_local_member_detail
{
static constexpr std::size_t Static_Alloc_Size = 4;

template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage;

template <class IndexType, class Allocator>
class index_pool;
}

template <class T, class Allocator>
class thread_local_member
{
public:
	thread_local_member();
	thread_local_member(Allocator& allocator);

	~thread_local_member();

	operator T& ();
	operator T& () const;

	template <std::enable_if_t<std::is_move_assignable_v<T>>* = nullptr>
	const T& operator=(T&& other);

	template <std::enable_if_t<std::is_copy_assignable_v<T>>* = nullptr>
	const T& operator=(const T& other);

private:
	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;

	static thread_local thread_local_member_detail::flexible_storage<T, Allocator> s_storage;
	static thread_local_member_detail::index_pool<std::uint32_t, Allocator> s_indexPool;

	allocator_type m_allocator;
	const std::uint32_t m_instanceIndex;
};

template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member()
	: m_instanceIndex(s_indexPool.get(m_allocator))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(Allocator& allocator)
	: m_allocator(allocator)
	, m_instanceIndex(s_indexPool.get(m_allocator))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member()
{
	s_indexPool.add(m_instanceIndex);
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& ()
{
	return s_storage.get_index(m_instanceIndex, m_allocator);
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& () const
{
	return s_storage.get_index(m_instanceIndex, m_allocator);
}

template<class T, class Allocator>
template <std::enable_if_t<std::is_copy_assignable_v<T>>*>
inline const T& thread_local_member<T, Allocator>::operator=(const T& other)
{
	T& myval(*this);
	myval = other;
	return *this;
}
template<class T, class Allocator>
template<std::enable_if_t<std::is_move_assignable_v<T>>*>
inline const T& thread_local_member<T, Allocator>::operator=(T&& other)
{
	T& myval(*this);
	myval = std::move(other);
	return *this;
}

// detail
namespace thread_local_member_detail
{
template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage
{
public:
	flexible_storage();
	~flexible_storage();

	T& get_index(std::uint32_t index, Allocator & allocator);

private:

	T* get_array_ref(std::uint32_t capacity, Allocator & allocator);
	void grow_to_size(std::uint32_t capacity, Allocator & allocator);

	void construct_static();
	void construct_dynamic(Allocator& allocator);
	void destroy_static();
	void destroy_dynamic();

	std::array<T, thread_local_member_detail::Static_Alloc_Size>& get_static();
	std::vector<T, Allocator>& get_dynamic();

	union
	{
		std::uint8_t m_staticStorage[sizeof(std::array<T, thread_local_member_detail::Static_Alloc_Size>)];
		std::uint8_t m_dynamicStorage[sizeof(std::vector<T, Allocator>)];
	};
	T* m_arrayRef;
	std::uint32_t m_capacity;
};

template<class T, class Allocator>
inline flexible_storage<T, Allocator>::flexible_storage()
	: m_staticStorage{}
	, m_capacity(0)
	, m_arrayRef(nullptr)
{
}
template<class T, class Allocator>
inline flexible_storage<T, Allocator>::~flexible_storage()
{
	if (!(Static_Alloc_Size < m_capacity)) {
		destroy_static();
	}
	else {
		destroy_dynamic();
	}
}
template<class T, class Allocator>
inline T& flexible_storage<T, Allocator>::get_index(std::uint32_t index, Allocator& allocator)
{
	if (!(index < m_capacity)) {
		m_arrayRef = get_array_ref(index + 1, allocator);
	}
	return m_arrayRef[index];
}
template<class T, class Allocator>
inline T* flexible_storage<T, Allocator>::get_array_ref(std::uint32_t capacity, Allocator& allocator)
{
	if (m_capacity < capacity) {
		grow_to_size(capacity, allocator);
	}
	if (!(Static_Alloc_Size < m_capacity)) {
		return &get_static()[0];
	}

	return &get_dynamic()[0];
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::grow_to_size(std::uint32_t capacity, Allocator& allocator)
{
	if (!(Static_Alloc_Size < capacity)) {
		if (!m_capacity) {
			construct_static();
		}
	}
	else{
		if (!(Static_Alloc_Size < m_capacity)) {
			construct_dynamic(allocator);
		}

		get_dynamic().resize(capacity);

	}
	m_capacity = capacity;
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::construct_static()
{
	new ((std::array<T, Static_Alloc_Size>*) & m_staticStorage[0]) std::array<T, Static_Alloc_Size>();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::construct_dynamic(Allocator& allocator)
{
	std::array<T, Static_Alloc_Size> src;

	if (m_capacity) {
		src = std::move(get_static());

		destroy_static();
	}
	
	new ((std::vector<T, Allocator>*) & m_dynamicStorage[0]) std::vector<T, Allocator>(m_capacity, allocator);
	get_dynamic().resize(0);

	for (std::size_t i = 0; i < m_capacity; ++i) {
		get_dynamic().emplace_back(std::move(src[i]));
	}
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_static()
{
	get_static().~array();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_dynamic()
{
	get_dynamic().~vector();
}
template<class T, class Allocator>
inline std::array<T, thread_local_member_detail::Static_Alloc_Size>& flexible_storage<T, Allocator>::get_static()
{
	return *reinterpret_cast<std::array<T, Static_Alloc_Size>*>(&m_staticStorage[0]);
}
template<class T, class Allocator>
inline std::vector<T, Allocator>& flexible_storage<T, Allocator>::get_dynamic()
{
	return *reinterpret_cast<std::vector<T, Allocator>*>(&m_dynamicStorage[0]);
}
template <class IndexType, class Allocator>
class index_pool
{
public:
	index_pool();
	~index_pool();

	IndexType get(Allocator& allocator);
	void add(IndexType index);

	struct node
	{
		node(IndexType index, shared_ptr<node> next)
			: m_index(index)
			, m_next(std::move(next))
		{}
		IndexType m_index;
		atomic_shared_ptr<node> m_next;
	};
	atomic_shared_ptr<node> m_top;

private:
	// Pre-allocate 'return entry' (no allocations in destructor)
	void push_pool_entry(Allocator& allocator);
	void push_pool_entry(shared_ptr<node> node);
	shared_ptr<node> get_pool_entry();

	atomic_shared_ptr<node> m_topPool;

	std::atomic<IndexType> m_iterator;
};
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::index_pool()
	: m_top(nullptr)
	, m_iterator(0)
{
}
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::~index_pool()
{
	shared_ptr<node> top(m_top.unsafe_load());
	while (top) {
		shared_ptr<node> next(top->m_next.unsafe_load());
		m_top.unsafe_store(next);
		top = std::move(next);
	}
}
template<class IndexType, class Allocator>
inline IndexType index_pool<IndexType, Allocator>::get(Allocator& allocator)
{
	shared_ptr<node> top(m_top.load(std::memory_order_relaxed));
	while (top) {
		raw_ptr<node> expected(top);
		if (m_top.compare_exchange_strong(expected, top->m_next.load(std::memory_order_acquire), std::memory_order_relaxed)) {

			const IndexType toReturn(top->m_index);

			push_pool_entry(std::move(top));

			return toReturn;
		}
	}

	push_pool_entry(allocator);

	return m_iterator.fetch_add(1, std::memory_order_relaxed);
}
template<class IndexType, class Allocator>
inline void index_pool<IndexType, Allocator>::add(IndexType index)
{
	shared_ptr<node> entry(get_pool_entry());
	entry->m_index = index;

	raw_ptr<node> expected;
	do {
		shared_ptr<node> top(m_top.load());
		expected = top.get_raw_ptr();
		entry->m_next.unsafe_store(std::move(top));
	} while (!m_top.compare_exchange_strong(expected, std::move(entry)));
}
template<class IndexType, class Allocator>
inline void index_pool<IndexType, Allocator>::push_pool_entry(Allocator& allocator)
{
	shared_ptr<node> entry(make_shared<node, Allocator>(allocator, std::numeric_limits<IndexType>::max(), nullptr));

	push_pool_entry(std::move(entry));
}
template<class IndexType, class Allocator>
inline void index_pool<IndexType, Allocator>::push_pool_entry(shared_ptr<node> entry)
{
	shared_ptr<node> toInsert(std::move(entry));

	raw_ptr<node> expected;
	do {
		shared_ptr<node> top(m_topPool.load(std::memory_order_acquire));
		expected = top.get_raw_ptr();
		toInsert->m_next = std::move(top);
	} while (!m_topPool.compare_exchange_strong(expected, std::move(toInsert)));
}
template<class IndexType, class Allocator>
inline shared_ptr<typename index_pool<IndexType, Allocator>::node> index_pool<IndexType, Allocator>::get_pool_entry()
{
	shared_ptr<node> top(m_topPool.load(std::memory_order_relaxed));
	while (top) {
		raw_ptr<node> expected(top);
		if (m_topPool.compare_exchange_strong(expected, top->m_next.load(std::memory_order_acquire), std::memory_order_relaxed)) {
			return top;
		}
	}
	throw std::runtime_error("Pre allocated entries should be 1:1 to fetched indices");
}
}
template <class T, class Allocator>
thread_local_member_detail::index_pool<std::uint32_t, Allocator> thread_local_member<T, Allocator>::s_indexPool;
template <class T, class Allocator>
thread_local thread_local_member_detail::flexible_storage<T, Allocator> thread_local_member<T, Allocator>::s_storage;

}