#pragma once

#include <vector>
#include <array>
#include <tuple>
#include <utility>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <thread>
#include <queue>

// Check if maybe arraygrow method needs dummy_entry instead of nullptr during item move.

namespace gdul
{

template <class T, class Allocator = std::allocator<T>>
class thread_local_member;

template <class T, class Allocator = std::allocator<T>>
using tlm = thread_local_member<T, Allocator>;

namespace tlm_detail
{
using size_type = std::uint32_t;

// The number items of placed directly in the thread_local storage space. 
// Beyond this count, new objects are instead allocated on an array. (->thread_local->array)
static constexpr size_type Static_Alloc_Size = 4;

template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage;

template <class T>
struct instance_tracker;

template <class Dummy = void>
class index_pool;

template <class T, size_type N, class ...Args>
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
	using size_type = typename tlm_detail::size_type;

	~thread_local_member();

	operator T& ();
	operator const T& () const;

	template <std::enable_if_t<std::is_move_assignable_v<T>> * = nullptr>
	const T& operator=(T&& other);

	template <std::enable_if_t<std::is_copy_assignable_v<T>> * = nullptr>
	const T& operator=(const T& other);

	bool operator==(const T& t) const;
	bool operator!=(const T& t) const;

	static void _unsafe_reset();
private:
	inline void check_for_invalidation() const;
	template <class ...Args>
	inline std::uint64_t store_tracked_instance(Args&& ...args) const;
	inline void refresh() const;
	inline void grow_instance_tracker_array() const;

	using instance_tracker_entry = shared_ptr<tlm_detail::instance_tracker<T>>;
	using instance_tracker_atomic_entry = atomic_shared_ptr<tlm_detail::instance_tracker<T>>;

	using instance_tracker_array = shared_ptr<instance_tracker_atomic_entry[]>;
	using instance_tracker_atomic_array = atomic_shared_ptr<instance_tracker_atomic_entry[]>;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
	using allocator_instance_tracker_array = typename std::allocator_traits<Allocator>::template rebind_alloc<instance_tracker_atomic_entry[]>;
	using allocator_instance_tracker_entry = typename std::allocator_traits<Allocator>::template rebind_alloc<tlm_detail::instance_tracker<T>>;

	struct tl_container
	{
		tl_container() 
			: m_iteration(0) {}

		tlm_detail::flexible_storage<T, allocator_type> m_items;
		std::uint64_t m_iteration;
	};
	struct st_container
	{
		st_container() 
			: m_nextIteration(0) {}

		tlm_detail::index_pool<> m_indexPool;

		instance_tracker_atomic_array m_instanceTrackers;
		instance_tracker_atomic_array m_swapArray;

		std::atomic<std::uint64_t> m_nextIteration;
	};

	static thread_local tl_container s_tl_container;
	static st_container s_st_container;

	const size_type m_index;

	// Must be 64 bit. May not overflow 
	const std::uint64_t m_iteration;

	mutable allocator_type m_allocator;
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
	, m_index(s_st_container.m_indexPool.get(allocator))
	, m_iteration(store_tracked_instance(std::forward<T&&>(init)))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(const Allocator& allocator, const T& init)
	: m_allocator(allocator)
	, m_index(s_st_container.m_indexPool.get(allocator))
	, m_iteration(store_tracked_instance(std::forward<const T&>(init)))
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member()
{
	s_st_container.m_instanceTrackers.load()[m_index].store(nullptr);
	s_st_container.m_indexPool.add(m_index);
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& ()
{
	check_for_invalidation();
	return s_tl_container.m_items[m_index];
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator const T& () const
{
	check_for_invalidation();
	return s_tl_container.m_items[m_index];
}
template<class T, class Allocator>
inline bool thread_local_member<T, Allocator>::operator==(const T& t) const
{
	return (operator const T&()).operator==(t);
}
template<class T, class Allocator>
inline bool thread_local_member<T, Allocator>::operator!=(const T& t) const
{
	return !operator==(t);
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::_unsafe_reset()
{
	s_st_container.m_indexPool.unsafe_reset();
	s_st_container.m_instanceTrackers.unsafe_store(nullptr);
	s_st_container.m_nextIteration = 0;
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
inline std::uint64_t thread_local_member<T, Allocator>::store_tracked_instance(Args&& ...args) const
{
	grow_instance_tracker_array();

	allocator_instance_tracker_entry alloc(m_allocator);
	instance_tracker_entry trackedEntry(make_shared<tlm_detail::instance_tracker<T>, allocator_instance_tracker_entry>(alloc, 0, std::forward<Args&&>(args)...));

	instance_tracker_array trackedInstances(nullptr);
	instance_tracker_array swapArray(nullptr);

	do {
		trackedInstances = s_st_container.m_instanceTrackers.load(std::memory_order_acquire);
		swapArray = s_st_container.m_swapArray.load(std::memory_order_relaxed);

		if (m_index < swapArray.item_count() &&
			swapArray[m_index] != trackedEntry) {
			swapArray[m_index].store(trackedEntry, std::memory_order_release);
		}
		if (trackedInstances[m_index] != trackedEntry) {
			trackedInstances[m_index].store(trackedEntry, std::memory_order_release);
		}
		// Just keep re-storing until the relation between m_swapArray / m_instanceTrackers has stabilized
	} while (s_st_container.m_swapArray != swapArray && s_st_container.m_instanceTrackers != trackedInstances);

	trackedEntry->m_iteration = s_st_container.m_nextIteration.operator++();

	return trackedEntry->m_iteration;
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::refresh() const
{
	const instance_tracker_array trackedInstances(s_st_container.m_instanceTrackers.load(std::memory_order_relaxed));
	const size_type items((size_type)trackedInstances.item_count());
	s_tl_container.m_items.reserve(items, m_allocator);

	for (size_type i = 0; i < items; ++i) {
		instance_tracker_entry instance(trackedInstances[i].load(std::memory_order_acquire));

		if (instance 
			&& ((s_tl_container.m_iteration < instance->m_iteration) 
			& !(m_iteration < instance->m_iteration))) {
			s_tl_container.m_items.reconstruct(i, instance->m_initArgs);
		}
	}

}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::grow_instance_tracker_array() const
{
	const size_type minimum(m_index + 1);
	instance_tracker_array activeArray(nullptr);
	do {
		activeArray = s_st_container.m_instanceTrackers.load();
		// If there is a fully committed array with enough entries, we can just break here.
		if (!(activeArray.item_count() < minimum)) {
			return;
		}

		instance_tracker_array swapArray(s_st_container.m_swapArray.load());

		if (swapArray.item_count() < minimum) {
			const float growth(((float)minimum) * 1.4f);

			allocator_instance_tracker_array arrayAlloc(m_allocator);
			instance_tracker_array grown(make_shared<instance_tracker_atomic_entry[], allocator_instance_tracker_array>((size_type)growth, arrayAlloc));

			raw_ptr<instance_tracker_atomic_entry[]> exp(swapArray);
			if (!s_st_container.m_swapArray.compare_exchange_strong(exp, std::move(grown))) {
				continue;
			}
			
			swapArray = s_st_container.m_swapArray.load();

			if (swapArray.item_count() < minimum) {
				continue;
			}
		}

		for (size_type i = 0, itemCount((size_type)activeArray.item_count()); i < itemCount; ++i) {
			// Only swap in if this entry is unaltered. (Leaving room for store(rs) to insert)
			raw_ptr<tlm_detail::instance_tracker<T>> exp(nullptr, 0);
			swapArray[i].compare_exchange_strong(exp, activeArray[i].load());
		}

		if (s_st_container.m_instanceTrackers.compare_exchange_strong(activeArray, swapArray)) {
			// We successfully swapped in and can now remove the swap-in entry
			raw_ptr<instance_tracker_atomic_entry[]> expSwap(swapArray);
			s_st_container.m_swapArray.compare_exchange_strong(expSwap, nullptr);
			break;
		}
	} while (activeArray.item_count() < minimum);
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
template <class T, class Allocator>
bool operator==(const tlm<T, Allocator>& tl, const T& t) {
	return tl.operator==(t);
}
template <class T, class Allocator>
bool operator!=(const tlm<T, Allocator>& tl, const T& t) {
	return tl.operator!=(t);
}
// detail
namespace tlm_detail
{
// Flexible storage keeps a few local slots, and if more are needed
// it starts using an allocating vector instead.
template <class T, class Allocator>
class alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage
{
public:
	flexible_storage() noexcept;
	~flexible_storage() noexcept;

	const T& operator[](size_type index) const noexcept;
	T& operator[](size_type index) noexcept;

	template <class ...Args>
	inline void reserve(size_type capacity, Allocator & allocator, Args && ...args);

	template <class ...Args>
	inline void reconstruct(size_type index, Args && ...args);

	inline size_type capacity() const noexcept;
private:


	T* get_array_ref() noexcept;

	template <class ...Args>
	void grow_to_size(size_type capacity, Allocator & allocator, Args && ...args);
	template <class ...Args>
	void construct_static(Args && ...args);
	template <class ...Args>
	void construct_dynamic(size_type capacity, Allocator & allocator, Args && ...args);
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
	size_type m_capacity;
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
inline const T& flexible_storage<T, Allocator>::operator[](size_type index) const noexcept
{
	return m_arrayRef[index];
}
template<class T, class Allocator>
inline T& flexible_storage<T, Allocator>::operator[](size_type index) noexcept
{
	return m_arrayRef[index];
}
template<class T, class Allocator>
template <class ...Args>
inline void flexible_storage<T, Allocator>::reserve(size_type capacity, Allocator& allocator, Args&& ...args)
{
	if (m_capacity < capacity) {
		grow_to_size(capacity, allocator, std::forward<Args&&>(args)...);

		m_arrayRef = get_array_ref();
	}
}
template<class T, class Allocator>
template <class ...Args>
inline void flexible_storage<T, Allocator>::reconstruct(size_type index, Args&& ...args)
{
	m_arrayRef[index].~T();
	new (&m_arrayRef[index]) T(std::forward<Args&&>(args)...);
}
template<class T, class Allocator>
inline size_type flexible_storage<T, Allocator>::capacity() const noexcept
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
inline void flexible_storage<T, Allocator>::grow_to_size(size_type capacity, Allocator& allocator, Args&& ...args)
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
inline void flexible_storage<T, Allocator>::construct_dynamic(size_type capacity, Allocator& allocator, Args&& ...args)
{
	std::vector<T, Allocator> replacement(capacity, std::forward<Args&&>(args)..., allocator);

	if (m_capacity) {
		for (size_type i = 0; i < m_capacity; ++i) {
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
template <class Dummy>
class index_pool
{
public:
	index_pool() noexcept;
	~index_pool() noexcept;

	void unsafe_reset();

	template <class Allocator>
	size_type get(Allocator allocator);
	void add(size_type index) noexcept;

	struct node
	{
		node(size_type index, shared_ptr<node> next) noexcept
			: m_index(index)
			, m_next(std::move(next))
		{}
		size_type m_index;
		atomic_shared_ptr<node> m_next;
	};

private:
	// Pre-allocate 'return entry' (so that adds can happen in destructor)
	template <class Allocator>
	void alloc_pool_entry(Allocator allocator);

	void push_pool_entry(shared_ptr<node> node);
	shared_ptr<node> get_pooled_entry();

	atomic_shared_ptr<node> m_topPool;
	atomic_shared_ptr<node> m_top;

	std::atomic<size_type> m_nextIndex;
};

template<class Dummy>
inline index_pool<Dummy>::index_pool() noexcept
	: m_topPool(nullptr)
	, m_top(nullptr)
	, m_nextIndex(0)
{
}
template<class Dummy>
inline index_pool<Dummy>::~index_pool() noexcept
{
}
template<class Dummy>
inline void index_pool<Dummy>::unsafe_reset()
{
	shared_ptr<node> top(m_top.unsafe_load());
	while (top)
	{
		shared_ptr<node> next(top->m_next.unsafe_load());
		m_top.unsafe_store(next);
		top = std::move(next);
	}

	m_nextIndex.store(0, std::memory_order_relaxed);
}
template<class Dummy>
template <class Allocator>
inline size_type index_pool<Dummy>::get(Allocator allocator)
{
	// Need to alloc new to counteract possible ABA issues (instead of possibly reusing top)
	alloc_pool_entry(allocator);

	shared_ptr<node> top(m_top.load(std::memory_order_relaxed));
	while (top) {
		if (m_top.compare_exchange_strong(top, top->m_next.load(), std::memory_order_relaxed)) {

			const size_type index(top->m_index);

			return index;
		}
	}
	return m_nextIndex.fetch_add(1, std::memory_order_relaxed);
}
template<class Dummy>
inline void index_pool<Dummy>::add(size_type index) noexcept
{
	shared_ptr<node> toInsert(get_pooled_entry());
	toInsert->m_index = index;

	shared_ptr<node> top(m_top.load());
	do{
		toInsert->m_next.store(top);
	} while (!m_top.compare_exchange_strong(top, std::move(toInsert)));
}
template<class Dummy>
template <class Allocator>
inline void index_pool<Dummy>::alloc_pool_entry(Allocator allocator)
{
	shared_ptr<node> entry(make_shared<node, Allocator>(allocator, std::numeric_limits<size_type>::max(), nullptr));
	push_pool_entry(entry);
}
template<class Dummy>
inline void index_pool<Dummy>::push_pool_entry(shared_ptr<node> entry)
{
	shared_ptr<node> toInsert(std::move(entry));
	shared_ptr<node> top(m_topPool.load(std::memory_order_acquire));
	do {
		toInsert->m_next.store(top);
	} while (!m_topPool.compare_exchange_strong(top, std::move(toInsert)));
}
template<class Dummy>
inline shared_ptr<typename index_pool<Dummy>::node> index_pool<Dummy>::get_pooled_entry()
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
	instance_tracker(std::uint64_t iteration, Args&&... args)
		: m_initArgs(std::forward<Args&&>(args)...)
		, m_iteration(iteration)
	{
	}

	const T m_initArgs;
	std::uint64_t m_iteration;
};

template<class T, size_type ...Ix, typename ...Args>
constexpr std::array<T, sizeof ...(Ix)> repeat(std::index_sequence<Ix...>, Args&&... args)
{
	return { {((void)Ix, T(std::forward<Args&&>(args)...))...} };
}
template <class T, size_type N, class ...Args>
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