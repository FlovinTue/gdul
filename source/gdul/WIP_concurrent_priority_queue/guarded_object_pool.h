#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <stdint.h>
#include <gdul/thread_local_member/thread_local_member.h>
#include <gdul/concurrent_queue/concurrent_queue.h>



// Perhaps push/pop pools and keep them in local caches?



// This could be merged into current implementation of conccurrent_object_pool. ! Could get real GUD!

namespace gdul {


template <class T, class Allocator>
class guarded_object_pool
{
public:

	using size_type = std::size_t;
	using allocator_type = Allocator;

	guarded_object_pool();
	guarded_object_pool(size_type tlCacheSize);
	guarded_object_pool(Allocator alloc);
	guarded_object_pool(Allocator alloc, size_type tlCacheSize);

	T* get();

	void reclaim_deferred(T* item);
	void reclaim(T* item);

	template <class Fn, class ...Args>
	std::invoke_result_t<Fn, Args...> guard(Fn f, Args&&... args);



private:
	struct alignas(std::hardware_destructive_interference_size) actor_index { std::size_t i; };

	tlm_detail::index_pool<void> m_indexPool;
	std::vector<actor_index> m_indices;


	struct tl_container
	{
		std::uint32_t index;
	};

	concurrent_queue<shared_ptr<T[]>> m_blocks; // Hmm want better cache layout. Allocate big block, and divide that into smaller tl caches
	concurrent_queue<shared_ptr<T*[]>> m_emptyCaches; // Same goes for these. Should be able to just make a big block that can hold all
	concurrent_queue<shared_ptr<T*[]>> m_fullCaches;

	tlm<tl_container> t_container;

	Allocator m_allocator;
};









}