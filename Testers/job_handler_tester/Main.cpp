// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "job_handler_tester.h"
#include <iostream>
#include <functional>
#include "../Common/timer.h"
#include <vld.h>
#include "../Common/tracking_allocator.h"

int main()
{	
	{
		gdul::job_handler_tester tester;

		
		gdul::job_handler_tester_info info;
		info.affinity = gdul::JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC;
		
		tester.init(info);

		const uint32_t scatterRuns(40);
		float scatterTimeAccum(0.f);
		std::size_t scatterBatchAccum(0);

		for (uint32_t i = 0; i < scatterRuns; ++i)
		{
			std::size_t arraySize(1500), batchSize(10);
			std::size_t bestBatchSize(0);
			float bestBatchTime(0.f);
			tester.run_scatter_test_input_output(arraySize, batchSize, bestBatchTime, bestBatchSize);
			scatterTimeAccum += bestBatchTime;
			scatterBatchAccum += bestBatchSize;
		}

		std::cout << "Best time / batchsize average: " << scatterTimeAccum / scatterRuns << ", " << scatterBatchAccum / scatterRuns << std::endl;
	
		for (uint32_t i = 0; i < 5000; ++i)
		{
			tester.run_consumption_strand_parallel_test(20000, 1.0f);
		}
	}

	std::cout << "Final allocated: " << gdul::s_allocated << std::endl;
	
	std::cout << "Hello World!\n";

}