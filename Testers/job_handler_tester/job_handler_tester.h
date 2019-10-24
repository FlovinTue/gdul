#pragma once

#include <gdul\job_handler_master.h>

namespace gdul
{
typedef job_handler tested_handler;
typedef job tested_work_unit;
typedef worker tested_worker;


enum JOB_HANDLER_TESTER_WORKER_AFFINITY
{
	JOB_HANDLER_TESTER_WORKER_AFFINITY_ASSIGNED,
	JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC,
	JOB_HANDLER_TESTER_WORKER_AFFINITY_MIXED,
};
struct job_handler_tester_info
{
	JOB_HANDLER_TESTER_WORKER_AFFINITY affinity;

	void(*workFunc)(void);
};
class job_handler_tester
{
public:
	job_handler_tester();
	~job_handler_tester();

	void init(const job_handler_tester_info& info);

	// Optimize for throughput 
	// In parallel
	// And in strand / single thread
	// Move a set of jobs through tests in lowest amount of time.

	// Todo: Find candidates for comparison

	float run_all_tests(std::size_t numInserts);

	float run_consumption_parallel_test(std::size_t numInserts);
	float run_construction_parallel_test(std::size_t numInserts);
	float run_mixed_parallel_test(std::size_t numInserts);
	float run_consumption_strand_test(std::size_t numInserts);

	void setup_workers();

	job_handler_tester_info myInfo;

	gdul::job_handler myHandler;
};

}