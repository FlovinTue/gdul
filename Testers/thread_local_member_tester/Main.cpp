
#include "pch.h"
#include <iostream>
#include <gdul\WIP_thread_local_member\thread_local_member.h>
#include <thread>
#include <vld.h>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <functional>
#include <vector>
#include "tester.h"
#include "../Common/Timer.h"
#include "no_opt.h"

int main()
{
	timer<float> time;

	{

	gdul::tester<int> tester(5);
	tester.test_index_pool(10000000);
	while (tester.m_worker.has_unfinished_tasks())
	{
		std::this_thread::yield();
	}

	}

	//no_opt check;
	//check.execute();

	std::cout << "allocated" << gdul::s_allocated << " and " << time.get() << " total time" << std::endl;
	return 0;
}