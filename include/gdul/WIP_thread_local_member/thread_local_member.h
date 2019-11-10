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

namespace tlm_detail
{
static constexpr std::size_t Static_Alloc_Size = 4;

template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage;

template <class Allocator>
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

	template <std::enable_if_t<std::is_move_assignable_v<T>> * = nullptr>
	const T& operator=(T&& other);

	template <std::enable_if_t<std::is_copy_assignable_v<T>> * = nullptr>
	const T& operator=(const T& other);

private:
	inline void check_for_invalidation() const;
	inline void store_current_iteration();
	inline void refresh() const;
	inline void grow_item_iterations_array();

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
	using allocator_size_t = typename std::allocator_traits<Allocator>::template rebind_alloc<std::size_t>;
	using allocator_size_t_array = typename std::allocator_traits<Allocator>::template rebind_alloc<std::size_t[]>;

	struct tl_container
	{
		tl_container() : m_iteration(0) {}
		tlm_detail::flexible_storage<T, allocator_type> m_items;
		std::size_t m_iteration;
	};
	struct st_container
	{
		st_container() : m_nextIteration(0) {}
		tlm_detail::index_pool<allocator_size_t> m_indexPool;
		atomic_shared_ptr<std::size_t[]> m_itemIterations;
		std::atomic<std::size_t> m_nextIteration;
	};

	static thread_local tl_container s_tl_container;
	static st_container s_st_container;

	mutable allocator_type m_allocator;

	const std::size_t m_index;
	const std::size_t m_iteration;
};

template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member()
	: m_index(s_st_container.m_indexPool.get(allocator_size_t(m_allocator)))
	, m_iteration(s_st_container.m_nextIteration.operator++())
{
	grow_item_iterations_array();
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(Allocator& allocator)
	: m_allocator(allocator)
	, m_index(s_st_container.m_indexPool.get(allocator_size_t(m_allocator)))
	, m_iteration(s_st_container.m_nextIteration.operator++())
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member()
{
	store_current_iteration();

	s_st_container.m_indexPool.add(m_index);
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& ()
{
	check_for_invalidation();
	return s_tl_container.m_items[m_index];
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& () const
{
	check_for_invalidation();
	return s_tl_container.m_items[m_index];
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::check_for_invalidation() const
{
	if (s_tl_container.m_iteration < m_iteration) {
		refresh();
		s_tl_container.m_iteration = m_iteration;
	}
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::store_current_iteration()
{
	shared_ptr<std::size_t[]> itemIterations(s_st_container.m_itemIterations.load());
	itemIterations[m_index] = m_iteration;
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::refresh() const
{
	const shared_ptr<std::size_t[]> itemIterations(s_st_container.m_itemIterations.load());
	const std::size_t items(s_tl_container.m_items.capacity());

	for (std::size_t i = 0; i < items; ++i) {
		const std::size_t currentItemIteration(itemIterations[i]);
		if ((s_tl_container.m_iteration < currentItemIteration) & !(m_iteration < currentItemIteration)) {
			s_tl_container.m_items.reconstruct(i);
		}
	}

	s_tl_container.m_items.reserve(itemIterations.item_count(), m_allocator);
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::grow_item_iterations_array()
{
	shared_ptr<std::size_t[]> itemIterations(s_st_container.m_itemIterations.load());
	const std::size_t minimum(m_index + 1);


	while (itemIterations.item_count() < minimum) {
		const float growth(((float)minimum) * 1.3f);

		allocator_size_t_array arrayAlloc(m_allocator);

		shared_ptr<std::size_t[]> grown(make_shared<std::size_t[], allocator_size_t_array>((std::size_t)growth, arrayAlloc));
		for (std::size_t i = 0; i < itemIterations.item_count(); ++i) {
			grown[i] = itemIterations[i];
		}

		if (s_st_container.m_itemIterations.compare_exchange_strong(itemIterations, std::move(grown))) {
			break;
		}
	}
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
namespace tlm_detail
{
// Flexible storage keeps a few local std::size_t slots, and if more are needed
// it starts using an allocating vector instead.
template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage
{
public:
	flexible_storage() noexcept;
	~flexible_storage() noexcept;

	const T& operator[](std::size_t index) const noexcept;
	T& operator[](std::size_t index) noexcept;

	template <class ...Args>
	inline void reserve(std::size_t capacity, Allocator & allocator, Args && ...args);

	template <class ...Args>
	inline void reconstruct(std::size_t index, Args && ...args);

	inline std::size_t capacity() const noexcept;
private:


	T* get_array_ref() noexcept;

	template <class ...Args>
	void grow_to_size(std::size_t capacity, Allocator & allocator, Args && ...args);
	template <class ...Args>
	void construct_static(Args && ...args);
	template <class ...Args>
	void construct_dynamic(Allocator & allocator, Args && ...args);
	void destroy_static() noexcept;
	void destroy_dynamic() noexcept;

	std::array<T, tlm_detail::Static_Alloc_Size>& get_static() noexcept;
	std::vector<T, Allocator>& get_dynamic() noexcept;

	union
	{
		std::uint8_t m_staticStorage[sizeof(std::array<T, tlm_detail::Static_Alloc_Size>)];
		std::uint8_t m_dynamicStorage[sizeof(std::vector<T, Allocator>)];
	};
	T* m_arrayRef;
	std::size_t m_capacity;
};

template<class T, class Allocator>
inline flexible_storage<T, Allocator>::flexible_storage() noexcept
	: m_staticStorage{}
	, m_capacity(0)
	, m_arrayRef(nullptr)
{
}
template<class T, class Allocator>
inline flexible_storage<T, Allocator>::~flexible_storage() noexcept
{
	if (!(Static_Alloc_Size < m_capacity)) {
		destroy_static();
	}
	else {
		destroy_dynamic();
	}
}
template<class T, class Allocator>
inline const T& flexible_storage<T, Allocator>::operator[](std::size_t index) const noexcept
{
	return m_arrayRef[index];
}
template<class T, class Allocator>
inline T& flexible_storage<T, Allocator>::operator[](std::size_t index) noexcept
{
	return m_arrayRef[index];
}
template<class T, class Allocator>
template <class ...Args>
inline void flexible_storage<T, Allocator>::reserve(std::size_t capacity, Allocator& allocator, Args&& ...args)
{
	if (m_capacity < capacity) {
		grow_to_size(capacity, allocator, std::forward<Args&&>(args)...);

		m_arrayRef = get_array_ref();
	}
}
template<class T, class Allocator>
template <class ...Args>
inline void flexible_storage<T, Allocator>::reconstruct(std::size_t index, Args&& ...args)
{
	m_arrayRef[index].~T();
	new (&m_arrayRef[index]) T(std::forward<Args&&>(args)...);
}
template<class T, class Allocator>
inline std::size_t flexible_storage<T, Allocator>::capacity() const noexcept
{
	return m_capacity;
}
template<class T, class Allocator>
inline T* flexible_storage<T, Allocator>::get_array_ref() noexcept
{
	if (!(Static_Alloc_Size < m_capacity)) {
		return &get_static()[0];
	}

	return &get_dynamic()[0];
}
template<class T, class Allocator>
template<class ...Args>
inline void flexible_storage<T, Allocator>::grow_to_size(std::size_t capacity, Allocator& allocator, Args&& ...args)
{
	if (!(Static_Alloc_Size < capacity)) {
		if (!m_capacity) {
			construct_static(std::forward<Args&&>(args)...);
		}
	}
	else {
		if (!(Static_Alloc_Size < m_capacity)) {
			construct_dynamic(allocator, std::forward<Args&&>(args)...);
		}

		get_dynamic().resize(capacity, std::forward<Args&&>(args)...);

	}
	m_capacity = capacity;
}
template<class T, class Allocator>
template<class ...Args>
inline void flexible_storage<T, Allocator>::construct_static(Args&& ...args)
{
	new ((std::array<T, Static_Alloc_Size>*) & m_staticStorage[0]) std::array<T, Static_Alloc_Size>();
	std::array<T, Static_Alloc_Size>& arr(get_static());

	for (std::size_t i = 0; i < Static_Alloc_Size; ++i) {
		arr[i].~T();
		new(&arr[i]) T(std::forward<Args&&>(args)...);
	}
}
template<class T, class Allocator>
template<class ...Args>
inline void flexible_storage<T, Allocator>::construct_dynamic(Allocator& allocator, Args&& ...args)
{
	std::array<T, Static_Alloc_Size> src;

	if (m_capacity) {
		src = std::move(get_static());

		destroy_static();
	}

	new ((std::vector<T, Allocator>*) & m_dynamicStorage[0]) std::vector<T, Allocator>(m_capacity, std::forward<Args&&>(args)..., allocator);
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_static() noexcept
{
	get_static().~array();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_dynamic() noexcept
{
	get_dynamic().~vector();
}
template<class T, class Allocator>
inline std::array<T, tlm_detail::Static_Alloc_Size>& flexible_storage<T, Allocator>::get_static() noexcept
{
	return *reinterpret_cast<std::array<T, Static_Alloc_Size>*>(&m_staticStorage[0]);
}
template<class T, class Allocator>
inline std::vector<T, Allocator>& flexible_storage<T, Allocator>::get_dynamic() noexcept
{
	return *reinterpret_cast<std::vector<T, Allocator>*>(&m_dynamicStorage[0]);
}
template <class Allocator>
class index_pool
{
public:
	index_pool() noexcept;
	~index_pool() noexcept;

	std::size_t get(Allocator allocator);
	void add(std::size_t index) noexcept;

	struct node
	{
		node(std::size_t index, shared_ptr<node> next) noexcept
			: m_index(index)
			, m_next(std::move(next))
		{}
		std::size_t m_index;
		atomic_shared_ptr<node> m_next;
	};

private:
	// Pre-allocate 'return entry' (so that adds can happen in destructor)
	void push_pool_entry(Allocator& allocator);
	void push_pool_entry(shared_ptr<node> node);
	shared_ptr<node> get_pooled_entry();

	atomic_shared_ptr<node> m_topPool;
	atomic_shared_ptr<node> m_top;

	std::atomic<std::size_t> m_nextIndex;
};
template<class Allocator>
inline index_pool<Allocator>::index_pool() noexcept
	: m_topPool(nullptr)
	, m_top(nullptr)
	, m_nextIndex(0)
{
}
template<class Allocator>
inline index_pool<Allocator>::~index_pool() noexcept
{
	shared_ptr<node> top(m_top.unsafe_load());
	while (top) {
		shared_ptr<node> next(top->m_next.unsafe_load());
		m_top.unsafe_store(next);
		top = std::move(next);
	}
}
template<class Allocator>
inline std::size_t index_pool<Allocator>::get(Allocator allocator)
{
	shared_ptr<node> top(m_top.load(std::memory_order_relaxed));
	while (top) {
		raw_ptr<node> expected(top);
		if (m_top.compare_exchange_strong(expected, top->m_next.load(std::memory_order_acquire), std::memory_order_relaxed)) {

			const std::size_t index(top->m_index);

			push_pool_entry(std::move(top));

			return index;
		}
	}

	push_pool_entry(allocator);

	return m_nextIndex.fetch_add(1, std::memory_order_relaxed);
}
template<class Allocator>
inline void index_pool<Allocator>::add(std::size_t index) noexcept
{
	shared_ptr<node> entry(get_pooled_entry());
	entry->m_index = index;

	raw_ptr<node> expected;
	do {
		shared_ptr<node> top(m_top.load());
		expected = top.get_raw_ptr();
		entry->m_next.unsafe_store(std::move(top));
	} while (!m_top.compare_exchange_strong(expected, std::move(entry)));
}
template<class Allocator>
inline void index_pool<Allocator>::push_pool_entry(Allocator& allocator)
{
	shared_ptr<node> entry(make_shared<node, Allocator>(allocator, std::numeric_limits<std::size_t>::max(), nullptr));

	push_pool_entry(std::move(entry));
}
template<class Allocator>
inline void index_pool<Allocator>::push_pool_entry(shared_ptr<node> entry)
{
	shared_ptr<node> toInsert(std::move(entry));

	raw_ptr<node> expected;
	do {
		shared_ptr<node> top(m_topPool.load(std::memory_order_acquire));
		expected = top.get_raw_ptr();
		toInsert->m_next = std::move(top);
	} while (!m_topPool.compare_exchange_strong(expected, std::move(toInsert)));
}
template<class Allocator>
inline shared_ptr<typename index_pool<Allocator>::node> index_pool<Allocator>::get_pooled_entry()
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
typename thread_local_member<T, Allocator>::st_container thread_local_member<T, Allocator>::s_st_container;
template <class T, class Allocator>
typename thread_local thread_local_member<T, Allocator>::tl_container thread_local_member<T, Allocator>::s_tl_container;
}