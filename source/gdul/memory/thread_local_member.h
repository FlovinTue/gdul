// Copyright(c) 2020 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <vector>
#include <array>
#include <atomic>
#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/containers/small_vector.h>

namespace gdul {

template <class T, class Allocator = std::allocator<T>>
class thread_local_member;

// Alias for thread_local_member
template <class T, class Allocator = std::allocator<T>>
using tlm = thread_local_member<T, Allocator>;

namespace tlm_detail {
using size_type = std::uint32_t;

// The max instances of one tlm type placed directly in the thread_local storage space (tlm->thread_local). 
// Beyond this count, new objects are instead allocated on an array (tlm->thread_local->array).
// May be altered.
static constexpr size_type StaticAllocSize = 3;

template <class T>
struct instance_tracker;
template <class T, class ...Args>
struct instance_tracker_constructor;

template <class Dummy>
class index_pool
{
public:
	index_pool() noexcept;
	~index_pool() noexcept
	{
		unsafe_reset();
	}

	void unsafe_reset();

	template <class Allocator>
	size_type get(Allocator allocator);
	void add(size_type index) noexcept;

	struct node
	{
		node(size_type index, shared_ptr<node> next) noexcept
			: m_index(index)
			, m_next(std::move(next))
		{
		}
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
inline void index_pool<Dummy>::unsafe_reset()
{
	shared_ptr<node> top(m_top.unsafe_load());
	while (top) 	{
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

			return top->m_index;
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
	do {
		toInsert->m_next.store(top);
	} while (!m_top.compare_exchange_strong(top, std::move(toInsert)));
}
template<class Dummy>
template <class Allocator>
inline void index_pool<Dummy>::alloc_pool_entry(Allocator allocator)
{
	shared_ptr<node> entry(gdul::allocate_shared<node>(allocator, std::numeric_limits<size_type>::max(), shared_ptr<node>(nullptr)));
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

template <class T, size_type ArraySize, class ...Args>
constexpr std::array<T, ArraySize> make_array(Args&&... args);

}
// Abstraction to enable members to be thread local. Internally, fast path contains 1 integer 
// comparison, and is potentially invalidated when the accessing thread sees a not-before seen object. 
// If the number of tlm instances of the same type (e.g tlm<int> != tlm<float>)
// does not exceed gdul::tlm_detail::StaticAllocSize, objects will be located in the corresponding
// thread's thread_local storage block(local->thread_local) else it will be mapped to an external
// array (local->thread_local->array). 
template <class T, class Allocator>
class thread_local_member
{
public:
	using value_type = T;
	using deref_type = std::remove_pointer_t<T>;
	using size_type = typename tlm_detail::size_type;

	/// <summary>
	/// Constructor
	/// </summary>
	thread_local_member();
	/// <summary>
	/// Construct with constructor args
	/// </summary>
	template <class ...Args>
	thread_local_member(Args&&... constructorArgs);
	/// <summary>
	/// Construct with allocator
	/// </summary>
	thread_local_member(Allocator allocator);
	/// <summary>
	/// Construct with allocator and constructor args
	/// </summary>
	template <class ...Args>
	thread_local_member(Allocator allocator, Args&&... constructorArgs);

	/// <summary>
	/// Destructor
	/// </summary>
	~thread_local_member() noexcept;

	/// <summary>
	/// Reconstruct
	/// </summary>
	inline void unsafe_reset();
	/// <summary>
	/// Reconstruct with new constructor args
	/// </summary>
	/// <param name="...constructorArgs">Arguments for type constructor</param>
	template <class ...Args>
	inline void unsafe_reset(Args&&... constructorArgs);
	/// <summary>
	/// Reconstruct with new allocator
	/// </summary>
	/// <param name="allocator">New allocator instance</param>
	inline void unsafe_reset(Allocator allocator);
	/// <summary>
	/// Reconstruct with new allocator and new constructor args
	/// </summary>
	/// <param name="allocator">New allocator instance</param>
	/// <param name="...constructorArgs">Arguments for type constructor</param>
	template <class ...Args>
	inline void unsafe_reset(Allocator allocator, Args&&... constructorArgs);

	inline operator T& ();
	inline operator const T& () const;

	// Main accessor. Use this to implement any additional methods/operators
	inline T& get();
	// Main accessor. Use this to implement any additional methods/operators
	inline const T& get() const;

	inline T& operator=(T&& other);
	inline T& operator=(const T& other);

	template <class Comp>
	inline bool operator==(const Comp& t) const;
	template <class Comp>
	inline bool operator!=(const Comp& t) const;

	inline bool operator==(const tlm<T>& other) const;
	inline bool operator!=(const tlm<T>& other) const;

	inline operator bool() const;

	inline deref_type* operator->();
	inline const deref_type* operator->() const;

	inline deref_type& operator*();
	inline const deref_type& operator*() const;

private:
	using instance_tracker_entry = shared_ptr<tlm_detail::instance_tracker<T>>;
	using instance_tracker_atomic_entry = atomic_shared_ptr<tlm_detail::instance_tracker<T>>;

	using instance_tracker_array = shared_ptr<instance_tracker_atomic_entry[]>;
	using instance_tracker_atomic_array = atomic_shared_ptr<instance_tracker_atomic_entry[]>;

	using allocator_type = typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
	using allocator_instance_tracker_array = typename std::allocator_traits<Allocator>::template rebind_alloc<instance_tracker_atomic_entry[]>;
	using allocator_instance_tracker_entry = typename std::allocator_traits<Allocator>::template rebind_alloc<tlm_detail::instance_tracker<T>>;

	inline void check_for_invalidation() const;
	inline void refresh() const;

	template <class ...Args>
	inline std::uint64_t initialize_instance_tracker(Args&& ...args) const;
	inline void store_instance_tracker(instance_tracker_entry entry) const noexcept;
	inline void grow_instance_tracker_array() const;

	struct tl_container
	{
		tl_container()
			: m_iteration(0)
		{
		}
		~tl_container()
		{
			for (std::size_t i = 0; i < m_constructed.size(); ++i) {
				if (m_constructed[i]) {
					m_constructed[i] = false;
					T* const item(reinterpret_cast<T*>(&m_items[i]));
					std::destroy_at(item);
				}
			}

			m_constructed.clear();
			m_items.clear();
		}

		using t_rep = std::aligned_storage_t<sizeof(T), alignof(T)>;

		small_vector<t_rep, tlm_detail::StaticAllocSize, allocator_type> m_items;
		small_vector<bool, tlm_detail::StaticAllocSize, allocator_type> m_constructed;

		std::uint64_t m_iteration;

#if defined _DEBUG // For natvis viewing
		const T* _dbgItemView = nullptr;
		std::size_t _dbgCapacity = 0;
#endif
	};
	struct st_container
	{
		st_container()
			: m_nextIteration(0)
		{
		}

		tlm_detail::index_pool<void> m_indexPool;

		instance_tracker_atomic_array m_instanceTrackers;
		instance_tracker_atomic_array m_swapArray;

		std::atomic<std::uint64_t> m_nextIteration;
	};

	static thread_local tl_container t_container;
	static st_container s_container;

	const size_type m_index;

	mutable allocator_type m_allocator;

	// Must be 64 bit. May not overflow 
	const std::uint64_t m_iteration;
};
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member()
	: thread_local_member<T, Allocator>::thread_local_member(T())
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(Allocator allocator)
	: thread_local_member<T, Allocator>::thread_local_member(T(), allocator)
{
}
template<class T, class Allocator>
template<class ...Args>
inline thread_local_member<T, Allocator>::thread_local_member(Args&&... constructorArgs)
	: thread_local_member<T, Allocator>::thread_local_member(Allocator(), std::forward<Args>(constructorArgs)...)
{
}
template<class T, class Allocator>
template<class ...Args>
inline thread_local_member<T, Allocator>::thread_local_member(Allocator allocator, Args&&... constructorArgs)
	: m_allocator(allocator)
	, m_index(s_container.m_indexPool.get(allocator))
	, m_iteration(initialize_instance_tracker(std::forward<Args>(constructorArgs)...))
{
	static_assert(std::is_default_constructible_v<T>, "Type must be default constructible");
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member() noexcept
{
	if (s_container.m_instanceTrackers) {
		store_instance_tracker(instance_tracker_entry(nullptr));
		s_container.m_indexPool.add(m_index);
	}
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::unsafe_reset()
{
	unsafe_reset(T());
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::unsafe_reset(Allocator allocator)
{
	unsafe_reset(Allocator(), T());
}
template<class T, class Allocator>
template<class ...Args>
inline void thread_local_member<T, Allocator>::unsafe_reset(Args && ...constructorArgs)
{
	unsafe_reset(Allocator(), std::forward<Args>(constructorArgs)...);
}
template<class T, class Allocator>
template<class ...Args>
inline void thread_local_member<T, Allocator>::unsafe_reset(Allocator allocator, Args && ...constructorArgs)
{
	thread_local_member* const addr(this);
	this->~thread_local_member();
	new (addr) thread_local_member(allocator, std::forward<Args>(constructorArgs)...);
}
template<class T, class Allocator>
template<class Comp>
inline bool thread_local_member<T, Allocator>::operator==(const Comp& t) const
{
	return get() == t;
}
template<class T, class Allocator>
template<class Comp>
inline bool thread_local_member<T, Allocator>::operator!=(const Comp& t) const
{
	return !operator==(t);
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& ()
{
	return get();
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator const T& () const
{
	return get();
}
template<class T, class Allocator>
inline bool thread_local_member<T, Allocator>::operator==(const tlm<T>& other) const
{
	return get() == other.get();
}
template<class T, class Allocator>
inline bool thread_local_member<T, Allocator>::operator!=(const tlm<T>& other) const
{
	return get() != other.get();
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator bool() const
{
	return get();
}
template<class T, class Allocator>
inline T& thread_local_member<T, Allocator>::get()
{
	check_for_invalidation();
	return *reinterpret_cast<T*>(&t_container.m_items[m_index]);
}
template<class T, class Allocator>
inline const T& thread_local_member<T, Allocator>::get() const
{
	check_for_invalidation();
	return *reinterpret_cast<const T*>(&t_container.m_items[m_index]);
}
template<class T, class Allocator>
inline typename thread_local_member<T, Allocator>::deref_type* thread_local_member<T, Allocator>::operator->()
{
	return get();
}
template<class T, class Allocator>
inline const typename thread_local_member<T, Allocator>::deref_type* thread_local_member<T, Allocator>::operator->() const
{
	return get();
}
template<class T, class Allocator>
inline typename thread_local_member<T, Allocator>::deref_type& thread_local_member<T, Allocator>::operator*()
{
	return *get();
}
template<class T, class Allocator>
inline const typename thread_local_member<T, Allocator>::deref_type& thread_local_member<T, Allocator>::operator*() const
{
	return *get();
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::check_for_invalidation() const
{
	if (t_container.m_iteration < m_iteration) {
		refresh();
		t_container.m_iteration = m_iteration;
	}
}
template<class T, class Allocator>
template<class ...Args>
inline std::uint64_t thread_local_member<T, Allocator>::initialize_instance_tracker(Args&& ...args) const
{
	grow_instance_tracker_array();

	allocator_instance_tracker_entry alloc(m_allocator);
	instance_tracker_entry trackedEntry(gdul::allocate_shared<tlm_detail::instance_tracker_constructor<T, Args...>>(alloc, std::forward<Args>(args)...));

	store_instance_tracker(trackedEntry);

	trackedEntry->m_iteration = s_container.m_nextIteration.operator++();

	return trackedEntry->m_iteration;
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::store_instance_tracker(instance_tracker_entry entry) const noexcept
{
	instance_tracker_array trackedInstances(nullptr);
	instance_tracker_array swapArray(nullptr);

	do {
		trackedInstances = s_container.m_instanceTrackers.load(std::memory_order_acquire);
		swapArray = s_container.m_swapArray.load(std::memory_order_relaxed);

		if (m_index < swapArray.item_count() &&
			swapArray[m_index] != entry) {
			swapArray[m_index].store(entry, std::memory_order_release);
		}
		if (trackedInstances[m_index] != entry) {
			trackedInstances[m_index].store(entry, std::memory_order_release);
		}
		// Just keep re-storing until the relation between m_swapArray / m_instanceTrackers has stabilized
	} while (s_container.m_swapArray != swapArray || s_container.m_instanceTrackers != trackedInstances);
}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::refresh() const
{
	const instance_tracker_array trackedInstances(s_container.m_instanceTrackers.load(std::memory_order_acquire));
	const size_type items((size_type)trackedInstances.item_count());
	t_container.m_items.resize(items);
	t_container.m_constructed.resize(items, false);

#if defined _DEBUG // For natvis viewing
	t_container._dbgItemView = static_cast<const T*>(t_container.m_items.data());
	t_container._dbgCapacity = t_container.m_items.capacity();
#endif

	for (size_type i = 0; i < items; ++i) {
		instance_tracker_entry instance(trackedInstances[i].load(std::memory_order_acquire));

		if (!instance && t_container.m_constructed[i]) {
			T* const item(reinterpret_cast<T*>(&t_container.m_items[i]));
			std::destroy_at(item);
			t_container.m_constructed[i] = false;
		}

		if (instance && ((t_container.m_iteration < instance->m_iteration) & !(m_iteration < instance->m_iteration))) {
			T* const item(reinterpret_cast<T*>(&t_container.m_items[i]));

			if (t_container.m_constructed[i]) {
				std::destroy_at(item);
				t_container.m_constructed[i] = false;
			}

			instance->construct_at(item);
			t_container.m_constructed[i] = true;
		}
	}

}
template<class T, class Allocator>
inline void thread_local_member<T, Allocator>::grow_instance_tracker_array() const
{
	const size_type minimum(m_index + 1);
	instance_tracker_array activeArray(nullptr);
	instance_tracker_array swapArray(nullptr);
	do {
		activeArray = s_container.m_instanceTrackers.load();

		if (!(activeArray.item_count() < minimum)) {
			break;
		}

		swapArray = s_container.m_swapArray.load();

		if (swapArray.item_count() < minimum) {
			const float growth(((float)minimum) * 1.4f);

			allocator_instance_tracker_array arrayAlloc(m_allocator);
			instance_tracker_array grown(gdul::allocate_shared<instance_tracker_atomic_entry[]>((size_type)growth, arrayAlloc));

			raw_ptr<instance_tracker_atomic_entry[]> exp(swapArray);
			s_container.m_swapArray.compare_exchange_strong(exp, std::move(grown));

			continue;
		}

		for (size_type i = 0, itemCount((size_type)activeArray.item_count()); i < itemCount; ++i) {
			raw_ptr<tlm_detail::instance_tracker<T>> exp(nullptr, 0);
			swapArray[i].compare_exchange_strong(exp, activeArray[i].load());
		}

		if (s_container.m_instanceTrackers.compare_exchange_strong(activeArray, swapArray)) {
			break;
		}
	} while (activeArray.item_count() < minimum);

	if (!(activeArray.item_count() < swapArray.item_count())) {
		raw_ptr<instance_tracker_atomic_entry[]> expSwap(swapArray);
		s_container.m_swapArray.compare_exchange_strong(expSwap, instance_tracker_array(nullptr));
	}
}
template<class T, class Allocator>
inline T& thread_local_member<T, Allocator>::operator=(const T& other)
{
	T& accessor(get());
	accessor = other;
	return accessor;
}
template<class T, class Allocator>
inline T& thread_local_member<T, Allocator>::operator=(T&& other)
{
	T& accessor(get());
	accessor = std::move(other);
	return accessor;
}
template <class T, class Allocator>
bool operator==(const T& t, const tlm<T, Allocator>& tl)
{
	return tl.operator==(t);
}
template <class T, class Allocator>
bool operator!=(const T& t, const tlm<T, Allocator>& tl)
{
	return tl.operator!=(t);
}
// detail
namespace tlm_detail {
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
template<class T, class Tuple, std::size_t ...IndexSeq>
void construct_with_tuple_expansion(T* mem, Tuple&& tup, std::index_sequence<IndexSeq...>)
{
	new (mem) T(std::get<IndexSeq>(std::forward<Tuple>(tup))...); tup;
}
template<class T, class Tuple>
void construct_with_tuple(T* mem, Tuple&& tup)
{
	using Indices = std::make_index_sequence<std::tuple_size<std::decay_t<Tuple>>::value>;
	construct_with_tuple_expansion(mem, std::forward<Tuple>(tup), Indices());
}
template <class T>
struct instance_tracker
{
	instance_tracker()
		: m_iteration(0)
	{
	}

	virtual ~instance_tracker() = default;
	virtual void construct_at(T* item) = 0;

	std::uint64_t m_iteration;
};
template <class T, class ...Args>
struct instance_tracker_constructor : public instance_tracker<T>
{
	instance_tracker_constructor(Args&&... args)
		: instance_tracker<T>::instance_tracker()
		, m_init(std::forward<Args>(args)...)
	{
	}

	~instance_tracker_constructor() = default;

	void construct_at(T* item) override final
	{
		construct_with_tuple(item, m_init);
	}

	std::tuple<Args...> m_init;
};
}
template <class T, class Allocator>
typename thread_local_member<T, Allocator>::st_container thread_local_member<T, Allocator>::s_container;
template <class T, class Allocator>
thread_local typename thread_local_member<T, Allocator>::tl_container thread_local_member<T, Allocator>::t_container;
}