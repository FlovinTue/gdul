


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
	gdul::sp_concurrent_chunk_pool<test_object> chunkPool(16);

	gdul::chunk_allocator<int*, decltype(chunkPool)> allocator(chunkPool);

	gdul::shared_ptr<test_object> sp = gdul::allocate_shared<test_object>(allocator);

	return 0;
}
