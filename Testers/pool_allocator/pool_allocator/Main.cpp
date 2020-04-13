


#include "pch.h"
#include <iostream>
#include <atomic>
#include <gdul/WIP_pool_allocator/pool_allocator.h>
#include <vld.h>



struct alignas(32) test_object
{
	int m_mem;
};

int main()
{
	gdul::memory_pool pool;

	constexpr std::size_t allocSharedSize(gdul::allocate_shared_size<test_object, gdul::pool_allocator<test_object>>());
	pool.init<allocSharedSize, alignof(test_object)>(8);

	gdul::pool_allocator<test_object> alloc(pool.create_allocator<test_object>());

	gdul::shared_ptr<test_object> sp(gdul::allocate_shared<test_object>(alloc));

	return 0;
}
