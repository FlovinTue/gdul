#pragma once

#include <gdul/execution/job_handler_master.h>
#include "work_tracker.h"

namespace gdul
{

enum JOB_HANDLER_TESTER_WORKER_AFFINITY
{
	JOB_HANDLER_TESTER_WORKER_AFFINITY_ASSIGNED = 1,
	JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC = 2,
	JOB_HANDLER_TESTER_WORKER_AFFINITY_MIXED = 4,
};
struct job_handler_tester_info
{
	JOB_HANDLER_TESTER_WORKER_AFFINITY affinity;
};
class job_handler_tester
{
public:
	job_handler_tester();
	~job_handler_tester();

	void init(const job_handler_tester_info& info);

	void basic_tests();

	void setup_workers();

	float run_consumption_parallel_test(std::size_t jobs, float overDuration);
	float run_consumption_strand_parallel_test(std::size_t jobs, float overDuration);
	float run_consumption_strand_test(std::size_t jobs, float overDuration);

	float run_predictive_scheduling_test();
	void run_scatter_test_input_output(std::size_t arraySize, std::size_t stepSize, float& outBestBatchTime);

	job_handler_tester_info m_info;

	gdul::job_async_queue m_asyncQueue;
	gdul::job_sync_queue m_syncQueue;

	gdul::job_handler m_handler;

	gdul::work_tracker m_work;

	std::vector<int*> m_scatterInput;
	std::vector<int*> m_scatterOutput;
};

}