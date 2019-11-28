// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "job_handler_tester.h"
#include <iostream>

namespace gdul
{
thread_local double testSum = static_cast<double>(rand() % 100);
}
void workFunc()
{
	std::size_t random((std::size_t)(rand() % 20) + 100ull);

	for (std::size_t i = 0; i < random;) {
		double toAdd(std::sqrt(gdul::testSum));
		gdul::testSum += toAdd;
		if (gdul::testSum) {
			++i;
		}
	}
}
int main()
{
	gdul::job_handler_tester tester;

	gdul::job_handler_tester_info info;
	info.affinity = gdul::JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC;

	tester.init(info);

	for (uint32_t i = 0; i < 50; ++i) {
		tester.run_consumption_strand_parallel_test(500, workFunc);
	}


	std::cout << "Hello World!\n";
}