#include "pch.h"
#include "tester.h"
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <../Testers/Common/Timer.h>
#include <../Testers/Common/thread_pool.h>

namespace gdul {

shared_ptr<thread_pool> g_tp;
thread_local std::vector<shared_ptr<test_struct>> tester::t_allocations;

tester::~tester() 
{
	g_tp->decommission();
}
void tester::init()
{
	g_tp = gdul::make_shared<thread_pool>();
	m_pool.init<allocate_shared_size<test_struct, pool_allocator<test_struct>>(), alignof(test_struct)>(16);
}

float tester::test_allocation(std::size_t iter)
{
	timer<float> time;
	
	for (std::size_t i = 0; i < iter; ++i) {
		g_tp->add_task([this, iter]() {do_allocate(iter);});
	}

	while (g_tp->has_unfinished_tasks())
		std::this_thread::yield();

	return time.get();
}

float tester::test_allocation_and_mem_access(std::size_t iter)
{
	return 0.0f;
}

float tester::test_deallocation(std::size_t iter)
{
	return 0.0f;
}

void tester::do_allocate(std::size_t iter)
{
	pool_allocator<test_struct> allocator(m_pool.create_allocator<test_struct>());

	for (std::size_t i = 0; i < iter; ++i) {
		t_allocations.push_back(gdul::allocate_shared<test_struct>(allocator));
	}

	t_allocations.clear();
}

}