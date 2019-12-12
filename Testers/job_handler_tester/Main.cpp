// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "job_handler_tester.h"
#include <iostream>
#include <functional>
#include "../Common/timer.h"
#include <vld.h>
#include "../Common/tracking_allocator.h"
#include <tuple>
#include <gdul/WIP_job_handler/scatter_job.h>

int main()
{	
	{
		gdul::job_handler_tester tester;
		
		gdul::job_handler_tester_info info;
		info.affinity = gdul::JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC;
		
		tester.init(info);
	
		for (uint32_t i = 0; i < 5000; ++i)
		{
			tester.run_consumption_strand_parallel_test(20000, 1.0f);
		}
	}

	std::cout << "Final allocated: " << gdul::s_allocated << std::endl;
	
	std::cout << "Hello World!\n";

}