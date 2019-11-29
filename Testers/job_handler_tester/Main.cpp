// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "job_handler_tester.h"
#include <iostream>
#include <functional>
#include "../Common/Timer.h"
#include <vld.h>
#include "../Common/tracking_allocator.h"

namespace gdul
{
thread_local double testSum = static_cast<double>(rand() % 100);
}
void workFunc()
{
	std::size_t random((std::size_t)(rand() % 200) + 1000ull);

	for (std::size_t i = 0; i < random;)
	{
		double toAdd(std::sqrt(gdul::testSum));
		gdul::testSum += toAdd;
		if (gdul::testSum)
		{
			++i;
		}
	}
}

int main()
{
	{
		gdul::job_handler_tester tester;

		gdul::job_handler_tester_info info;
		info.affinity = gdul::JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC;

		tester.init(info);

		for (uint32_t i = 0; i < 5000; ++i)
		{
			tester.run_consumption_strand_parallel_test(5000, workFunc);
		}
	}

	std::cout << "Final allocated: " << gdul::s_allocated << std::endl;

	std::cout << "Hello World!\n";
}