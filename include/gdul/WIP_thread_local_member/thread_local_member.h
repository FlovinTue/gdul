#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <utility>
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

template <class T>
struct instance_tracker;

template <class Allocator>
class index_pool;

template <class T, std::size_t N, class ...Args>
constexpr std::array<T, N> make_array(Args&&... args);

}

template <class T, class Allocator>
class thread_local_member
{
public:
	thread_local_member();
	thread_local_member(T&& init);
	thread_local_member(const T& init);
	thread_local_member(const Allocator& allocator);
	thread_local_member(const Allocator& allocator, T&& init);
	thread_local_member(const Allocator& allocator, const T& init);

	using value_type = T;

	~thread_local_member();

	operator T& ();
	operator T& () const;

	template <std::enable_if_t<std::is_move_assignable_v<T>> * = nullptr>
	const T& operator=(T&& other);

	template <std::enable_if_t<std::is_copy_assignable_v<T>> * = nullptr>
	const T& operator=(const T& other);

private:
	inline void check_for_invalidation() const;
	template <class ...Args>
	inline std::size_t store_tracked_instance(Args&& ...args) const;
	inline void refresh() const;
	inline void grow_instance_tracker_array() const;

	using instance_tracker_entry = shared_ptr<tlm_detail::instance_tracker<T>>;
	using instance_tracker_atomic_entry = atomic_shared_ptr<tlm_detail::instance_tracker<T>>;

	using instance_tracker_array = shared_ptr<instance_tracker_atomic_entry[]>;
	using instance_tracker_atomic_array = atomic_shared_ptr<instance_tracker_atomic_entry[]>;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
	using allocator_size_t = typename std::allocator_traits<Allocator>::template rebind_alloc<std::size_t>;
	using allocator_instance_tracker_array = typename std::allocator_traits<Allocator>::template rebind_alloc<instance_tracker_atomic_entry[]>;
	using allocator_instance_tracker_entry = typename std::allocator_traits<Allocator>::template rebind_alloc<tlm_detail::instance_tracker<T>>;

	struct tl_container
	{
		tl_container() 
			: m_iteration(0) {}

		tlm_detail::flexible_storage<T, allocator_type> m_items;
		std::size_t m_iteration;
	};
	struct st_container
	{
		st_container() 
			: m_nextIteration(0) {}

		tlm_detail::index_pool<allocator_size_t> m_indexPool;
		instance_tracker_atomic_array m_instanceTrackers;
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
	: thread_local_member<T, Allocator>::thread_local_member(T())
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(T&& init)
	: thread_local_member<T, Allocator>::thread_local_member(Allocator(), std::move(init))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(const T& init)
	: thread_local_member<T, Allocator>::thread_local_member(Allocator(), init)
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(const Allocator& allocator)
	: thread_local_member<T, Allocator>::thread_local_member(allocator, T())
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(const Allocator& allocator, T&& init)
	: m_allocator(allocator)
	, m_index(s_st_container.m_indexPool.get(allocator_size_t(m_allocator)))
	, m_iteration(store_tracked_instance(std::forward<T&&>(init)))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(const Allocator& allocator, const T& init)
	: m_allocator(allocator)
	, m_index(s_st_container.m_indexPool.get(allocator_size_t(m_allocator)))
	, m_iteration(store_tracked_instance(std::forward<const T&>(init)))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member()
{
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
template <class ...Args>
inline std::size_t thread_local_member<T, Allocator>::store_tracked_instance(Args&& ...args) const
{
	grow_instance_tracker_array();

	instance_tracker_array itemIterations(s_st_container.m_instanceTrackers.load(std::memory_order_relaxed));

	allocator_instance_tracker_entry alloc(m_allocator);

	instance_tracker_entry trackedEntry(make_shared<tlm_detail::instance_tracker<T>, allocator_instance_tracker_entry>(alloc, 0, std::forward<Args&&>(args)...));

	itemIterations[m_index].store(trackedEntry, std::memory_order_release);

	trackedEntry->m_iteration = s_st_container.m_nextIteration.operator++();

	return trackedEntry->m_iteration;
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::refresh() const
{
	const instance_tracker_array trackedInstances(s_st_container.m_instanceTrackers.load(std::memory_order_relaxed));
	const std::size_t items(trackedInstances.item_count());

	for (std::size_t i = 0; i < items; ++i) {
		instance_tracker_entry instance(trackedInstances[i].load(std::memory_order_acquire));

		if (instance 
			&& ((s_tl_container.m_iteration < instance->m_iteration) 
			& !(m_iteration < instance->m_iteration))) {
			s_tl_container.m_items.reserve(i + 1, m_allocator);
			s_tl_container.m_items.reconstruct(i, instance->m_initArgs);
		}
	}

}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::grow_instance_tracker_array() const
{
	instance_tracker_array trackedInstances(s_st_container.m_instanceTrackers.load(std::memory_order_relaxed));
	const std::size_t minimum(m_index + 1);

	allocator_instance_tracker_array arrayAlloc(m_allocator);

	while (trackedInstances.item_count() < minimum) {
		const float growth(((float)minimum) * 1.3f);

		instance_tracker_array grown(make_shared<instance_tracker_atomic_entry[], allocator_instance_tracker_array>((std::size_t)growth, arrayAlloc));
		for (std::size_t i = 0; i < trackedInstances.item_count(); ++i) {
			grown[i].unsafe_store(trackedInstances[i].load(std::memory_order_relaxed), std::memory_order_relaxed);
		}

		if (s_st_container.m_instanceTrackers.compare_exchange_strong(trackedInstances, std::move(grown))) {
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
	void construct_dynamic(std::size_t capacity, Allocator & allocator, Args && ...args);
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
			construct_dynamic(capacity, allocator, std::forward<Args&&>(args)...);
		}

		get_dynamic().resize(capacity, std::forward<Args&&>(args)...);

	}
	m_capacity = capacity;
}
template<class T, class Allocator>
template<class ...Args>
inline void flexible_storage<T, Allocator>::construct_static(Args&& ...args)
{
	new ((std::array<T, Static_Alloc_Size>*) & m_staticStorage[0]) std::array<T, Static_Alloc_Size>(make_array<T, Static_Alloc_Size>(std::forward<Args&&>(args)...));

}
template<class T, class Allocator>
template<class ...Args>
inline void flexible_storage<T, Allocator>::construct_dynamic(std::size_t capacity, Allocator& allocator, Args&& ...args)
{
	std::vector<T, Allocator> replacement(capacity, std::forward<Args&&>(args)..., allocator);

	if (m_capacity) {
		for (std::size_t i = 0; i < m_capacity; ++i) {
			replacement[i] = std::move(m_arrayRef[i]);
		}

		destroy_static();
	}

	new ((std::vector<T, Allocator>*) & m_dynamicStorage[0]) std::vector<T, Allocator>(std::move(replacement));
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
	void alloc_pool_entry(Allocator& allocator);
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
	allocator;
	shared_ptr<node> top(m_top.load(std::memory_order_relaxed));
	while (top) {
		if (m_top.compare_exchange_strong(top, top->m_next.load(std::memory_order_acquire), std::memory_order_relaxed)) {

			const std::size_t index(top->m_index);
			
			top->m_next = nullptr;

			push_pool_entry(std::move(top));

			return index;
		}
	}

	alloc_pool_entry(allocator);

	return m_nextIndex.fetch_add(1, std::memory_order_relaxed);
}
template<class Allocator>
inline void index_pool<Allocator>::add(std::size_t index) noexcept
{
	shared_ptr<node> toInsert(get_pooled_entry());
	toInsert->m_index = index;
	shared_ptr<node> top(m_top.load());
	do{
		toInsert->m_next.store(top);
	} while (!m_top.compare_exchange_strong(top, std::move(toInsert)));
}
template<class Allocator>
inline void index_pool<Allocator>::alloc_pool_entry(Allocator& allocator)
{
	shared_ptr<node> entry(make_shared<node, Allocator>(allocator, std::numeric_limits<std::size_t>::max(), nullptr));

	push_pool_entry(std::move(entry));
}
template<class Allocator>
inline void index_pool<Allocator>::push_pool_entry(shared_ptr<node> entry)
{
	shared_ptr<node> toInsert(std::move(entry));

	shared_ptr<node> top(m_topPool.load(std::memory_order_acquire));
	do {
		toInsert->m_next.store(top);
	} while (!m_topPool.compare_exchange_strong(top, std::move(toInsert)));
}
template<class Allocator>
inline shared_ptr<typename index_pool<Allocator>::node> index_pool<Allocator>::get_pooled_entry()
{
	shared_ptr<node> top(m_topPool.load(std::memory_order_relaxed));
	
	while (top) {
		if (m_topPool.compare_exchange_strong(top, top->m_next.load(std::memory_order_acquire))) {
			return top;
		}
	}
	throw std::runtime_error("Pre allocated entries should be 1:1 to fetched indices");
}
template <class T>
struct instance_tracker
{
	template <class ...Args>
	instance_tracker(std::size_t iteration, Args&&... args)
		: m_initArgs(std::forward<Args&&>(args)...)
		, m_iteration(iteration)
	{
	}

	const T m_initArgs;
	std::size_t m_iteration;
};

template<class T, std::size_t ...Ix, typename ...Args>
constexpr std::array<T, sizeof ...(Ix)> repeat(std::index_sequence<Ix...>, Args&&... args)
{
	return { {((void)Ix, T(std::forward<Args&&>(args)...))...} };
}
template <class T, std::size_t N, class ...Args>
constexpr std::array<T, N> make_array(Args&&... args)
{
	return std::array<T, N>(repeat<T>(std::make_index_sequence<N>(), std::forward<Args&&>(args)...));
}
}
template <class T, class Allocator>
typename thread_local_member<T, Allocator>::st_container thread_local_member<T, Allocator>::s_st_container;
template <class T, class Allocator>
typename thread_local thread_local_member<T, Allocator>::tl_container thread_local_member<T, Allocator>::s_tl_container;
}