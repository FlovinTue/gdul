// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "job_handler_tester.h"
#include <iostream>
#include <functional>
#include <vld.h>
#include "../Common/tracking_allocator.h"
#include <gdul/job_handler/tracking/job_graph.h>

int main()
{
	{
		gdul::job_handler_tester tester;


		gdul::job_handler_tester_info info;
		info.affinity = gdul::JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC;

		tester.init(info);

		tester.basic_tests();

		const uint32_t scatterRuns(20);
		float scatterTimeAccum(0.f);

		for (uint32_t i = 0; i < scatterRuns; ++i) {
			std::size_t arraySize(1500), batchSize(10);
			float bestBatchTime(0.f);
			tester.run_scatter_test_input_output(arraySize, batchSize, bestBatchTime);
			scatterTimeAccum += bestBatchTime;
		}

		std::cout << "Best time: " << scatterTimeAccum / scatterRuns << std::endl;

		for (uint32_t i = 0; i < 5; ++i) {
			tester.run_consumption_strand_parallel_test(1500, 1.0f);
		}

		{
			float predictiveMin(FLT_MAX);
			float predictiveMax(-FLT_MAX);
			float predictiveAccum(0.f);
			const uint32_t predictiveIter(800);
			for (auto i = 0; i < predictiveIter; ++i) {
				const float result = tester.run_predictive_scheduling_test();
				predictiveAccum += result;
				predictiveMin = std::min(result, predictiveMin);
				predictiveMax = std::max(result, predictiveMax);
			}

			std::cout << "\n\nFinished predictive testing batch.\nAverage: " << predictiveAccum / (float)predictiveIter * 1000.f << " ms" << "\nMin: " << predictiveMin * 1000.f << " ms" << "\nMax: " << predictiveMax * 1000.f << " ms" << "\n\n" << std::endl;
		}


#if defined (GDUL_JOB_DEBUG)
		tester.m_handler.dump_job_graph("");
		tester.m_handler.dump_job_time_sets("");
#endif
	}

	std::cout << "Final allocated: " << gdul::s_allocated << std::endl;

	std::cin.get();
}