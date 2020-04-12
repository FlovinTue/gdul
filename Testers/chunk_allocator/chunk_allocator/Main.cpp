


#include "pch.h"
#include <iostream>
#include <atomic>
#include <gdul/WIP_chunk_allocator/chunk_allocator.h>




struct alignas(32) test_object
{
	int m_mem;
};

int main()
{
	gdul::memory_chunk_pool pool;

	constexpr std::size_t allocSharedSize(gdul::allocate_shared_size<test_object, gdul::chunk_allocator<test_object>>());
	pool.init<allocSharedSize, alignof(test_object)>(8);

	gdul::chunk_allocator<test_object> alloc(pool.create_allocator<test_object>());

	gdul::shared_ptr<test_object> sp(gdul::allocate_shared<test_object>(alloc));

	return 0;
}
