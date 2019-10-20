#include "job_handler_tester.h"



job_handler_tester::job_handler_tester()
{
}


job_handler_tester::~job_handler_tester()
{
}

void job_handler_tester::init()
{
	myHandler.Init();
}

float job_handler_tester::run_all_tests(std::size_t numInserts)
{
	const float parallelConsume(run_consumption_parallel_test(numInserts));
	const float parallelConstru(run_construction_parallel_test(numInserts));
	const float parallelMixed(run_mixed_parallel_test(numInserts));
	const float strandConsume(run_consumption_strand_test(numInserts));

	return parallelConsume + parallelConstru + parallelMixed + strandConsume;
}

float job_handler_tester::run_consumption_parallel_test(std::size_t numInserts)
{
	numInserts;

	// Reset
	// Init ( max threads )

	// Create root

	// LOOP
	// Create 1 - 8 children
	// ------
	// Create end

	// Enable root
	// Wait for end

	return 0.0f;
}

float job_handler_tester::run_construction_parallel_test(std::size_t numInserts)
{
	numInserts;
	// Reset
	// Init ( 1 threads )

	// Create root

	// 2 threads building trees
	// 6 threads building jobs depending on latest in each tree
	// Create end

	// Enable root
	// Wait for end
	return 0.0f;
}

float job_handler_tester::run_mixed_parallel_test(std::size_t numInserts)
{
	numInserts;

	// Reset
	// Init ( max threads )

	// Create root
	// Enable root

	// 2 threads building trees
	// 6 threads building jobs depending on latest in each tree

	// Create end

	// Wait for end

	return 0.0f;
}

float job_handler_tester::run_consumption_strand_test(std::size_t numInserts)
{
	numInserts;

	// Reset
	// Init ( max threads )

	// Create root

	// LOOP
	// Create 1 children
	// ------
	// Create end

	// Enable root
	// Wait for end

	return 0.0f;
}

