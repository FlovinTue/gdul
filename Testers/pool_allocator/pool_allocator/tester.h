#pragma once

#include <gdul/pool_allocator/pool_allocator.h>

namespace gdul {

struct test_struct
{
	std::size_t m_counter;	
};

class tester
{
public:
	~tester();

	void init();

	float test_allocation(std::size_t iter);
	float test_allocation_and_mem_access(std::size_t iter);
	float test_deallocation(std::size_t iter);



private:
	void do_allocate(std::size_t iter);

	static thread_local std::vector<shared_ptr<test_struct>> t_allocations;

	memory_pool m_pool;
};
}

