
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

	for(uint32_t i =0; i < 100; ++i)
	{
		{
			gdul::tester<int> tester(5);
			tester.test_index_pool(100000);
			while (tester.m_worker.has_unfinished_tasks())
			{
				std::this_thread::yield();
			}
		}
	std::cout << "allocated" << gdul::s_allocated << " and " << time.get() << " total time" << std::endl;
	}

	gdul::doPrint = true;

	return 0;
}