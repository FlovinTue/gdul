#pragma once

#include <gdul\job_handler_master.h>

class job_handler_tester
{
public:
	job_handler_tester();
	~job_handler_tester();

	void init();

	// Optimize for throughput 
	// In parallel
	// And in strand / single thread
	// Move a set of jobs through tests in lowest amount of time.

	// Todo: Find candidates for comparison

	float run_all_tests(std::size_t numInserts);

	float run_consumption_parallel_test(std::size_t numInserts);
	float run_construction_parallel_test(std::size_t numInserts);
	float run_mixed_parallel_test(std::size_t numInserts);

	float run_construction_strand_test(std::size_t numInserts);
	float run_consumption_strand_test(std::size_t numInserts);

	gdul::job_handler myHandler;
};

