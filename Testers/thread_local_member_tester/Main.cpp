
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

struct int_iter
{
	int_iter()
	{
		m_iter = ++iters;
	}
	int_iter(int i) { m_iter = i; }

	bool operator!=(const int_iter& other) const { return other.m_iter != m_iter; }
	bool operator==(const int_iter& other) const { return other.m_iter == m_iter; }

	static int iters;
	int m_iter;
};
int int_iter::iters = 0;

int main()
{
	timer<float> time;

	for(uint32_t i =0; i < 200; ++i)
	{
		{
			gdul::tester<int_iter> tester(int_iter{ 5 });
			tester.execute(); 

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