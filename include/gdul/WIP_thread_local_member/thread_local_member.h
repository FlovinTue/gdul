#pragma once

#include <vector>
#include <array>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>

namespace gdul
{

namespace thread_local_member_detail
{
static constexpr std::size_t Static_Alloc_Size = 4;

template <class T, class Allocator>
union alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage;

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

private:

	T* get_array_ref();

	static thread_local thread_local_member_detail::flexible_storage s_storage;
	static thread_local T* s_arrayRef;
	static thread_local_member_detail::index_pool<std::uint32_t, Allocator> s_indexPool;

	const std::uint32_t m_instanceIndex;
	Allocator m_allocator;
};

template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member()
{
}

template<class T, class Allocator>
inline thread_local_member<T, Allocator>::thread_local_member(Allocator& allocator)
	: m_allocator(allocator)
{
}

template<class T, class Allocator>
inline thread_local_member<T, Allocator>::~thread_local_member()
{
}


template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& ()
{
}
template<class T, class Allocator>
inline thread_local_member<T, Allocator>::operator T& () const
{
}
template<class T, class Allocator>
inline T* thread_local_member<T, Allocator>::get_array_ref()
{
	return NULL;
}

// detail
namespace thread_local_member_detail
{
template <class T, class Allocator>
union alignas(alignof(T) < 8 ? 8 : alignof(T)) flexible_storage
{
	flexible_storage();

	void construct_static();
	void construct_dynamic(Allocator allocator);
	void destroy_static();
	void destroy_dynamic(Allocator allocator);

	static thread_local std::size_t s_capacity;

	std::array<T, thread_local_member_detail::Static_Alloc_Size>& get_static();
	std::vector<T, Allocator>& get_dynamic();

	std::uint8_t myStaticStorage[sizeof(std::array<T, thread_local_member_detail::Static_Alloc_Size>];
	std::uint8_t myDynamicStorage[sizeof(std::vector<T, Allocator>)];
};

template<class T, class Allocator>
inline flexible_storage<T, Allocator>::flexible_storage()
{
	construct_static();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::construct_static()
{
	new ((std::array<T, Static_Alloc_Size>*) & myStaticStorage[0]) std::array<T, Static_Alloc_Size>();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::construct_dynamic(Allocator allocator)
{
	std::array<T, Static_Alloc_Size> move(std::move(get_static()));

	destroy_static();

	new ((std::vector<T, Allocator>*) & myDynamicStorage[0]) std::vector<T, Allocator>(allocator);
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_static()
{
	get_static().~array();
}
template<class T, class Allocator>
inline void flexible_storage<T, Allocator>::destroy_dynamic(Allocator allocator)
{
	get_dynamic().~vector();
}
template<class T, class Allocator>
inline std::array<T, thread_local_member_detail::Static_Alloc_Size>& flexible_storage<T, Allocator>::get_static()
{
	return *reinterpret_cast<std::array<T, Static_Alloc_Size>*>(&myStaticStorage[0]);
}
template<class T, class Allocator>
inline std::vector<T, Allocator>& flexible_storage<T, Allocator>::get_dynamic()
{
	return *reinterpret_cast<std::vector<T, Allocator>*>(&myDynamicStorage[0]);
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
			: myIndex(index)
			, myNext(std::move(next))
		{
		}
		IndexType myIndex;
		atomic_shared_ptr<node> myNext;
	};
	atomic_shared_ptr<node> myTop;

private:
	// Pre-allocate 'return entry' (no allocations in destructor)
	void push_pool_entry(Allocator& allocator);
	void push_pool_entry(shared_ptr<node> node);
	shared_ptr<node> get_pool_entry();

	atomic_shared_ptr<node> myTopPool;

	std::atomic<IndexType> myIterator;
};
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::index_pool()
	: myTop(nullptr)
	, myIterator(0)
{
}
template<class IndexType, class Allocator>
inline index_pool<IndexType, Allocator>::~index_pool()
{
	shared_ptr<node> top(myTop.unsafe_load());
	while (top) {
		shared_ptr<node> next(top->myNext.unsafe_load());
		myTop.unsafe_store(next);
		top = std::move(next);
	}
}
template<class IndexType, class Allocator>
inline IndexType index_pool<IndexType, Allocator>::get(Allocator& allocator)
{
	shared_ptr<node> top(myTop.load(std::memory_order_relaxed));
	while (top) {
		raw_ptr<node> expected(top);
		if (myTop.compare_exchange_strong(expected, top->myNext.load(std::memory_order_acquire), std::memory_order_relaxed)) {

			const IndexType toReturn(top->myIndex);

			push_pool_entry(std::move(top));

			return toReturn;
		}
	}

	push_pool_entry(allocator);

	return myIterator++;
}
template<class IndexType, class Allocator>
inline void index_pool<IndexType, Allocator>::add(IndexType index)
{
	shared_ptr<node> entry(get_pool_entry());
	entry->myIndex = index;

	raw_ptr<node> expected;
	do {
		shared_ptr<node> top(myTop.load());
		expected = top.get_raw_ptr();
		entry->myNext.unsafe_store(std::move(top));
	} while (!myTop.compare_exchange_strong(expected, std::move(entry)));
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
		shared_ptr<node> top(myTopPool.load(std::memory_order_acquire));
		expected = top.get_raw_ptr();
		toInsert->myNext = std::move(top);
	} while (!myTopPool.compare_exchange_strong(expected, std::move(toInsert)));
}
template<class IndexType, class Allocator>
inline shared_ptr<typename index_pool<IndexType, Allocator>::node> index_pool<IndexType, Allocator>::get_pool_entry()
{
	shared_ptr<node> top(myTopPool.load(std::memory_order_relaxed));
	while (top) {
		raw_ptr<node> expected(top);
		if (myTopPool.compare_exchange_strong(expected, top->myNext.load(std::memory_order_acquire), std::memory_order_relaxed)) {
			return top;
		}
	}
	throw std::runtime_error("Pre allocated entries should be 1:1 to fetched indices");
}
}
}