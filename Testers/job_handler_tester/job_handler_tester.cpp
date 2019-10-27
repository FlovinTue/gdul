#include "job_handler_tester.h"
#include <string>

namespace gdul
{

job_handler_tester::job_handler_tester()
	: myInfo()
{
}


job_handler_tester::~job_handler_tester()
{
	myHandler.retire_workers();
}

void job_handler_tester::init(const job_handler_tester_info& info)
{
	myInfo = info;

	setup_workers();
}
void job_handler_tester::setup_workers()
{
	const std::size_t maxWorkers(std::thread::hardware_concurrency());

	std::size_t dynamicWorkers(0);
	std::size_t staticWorkers(0);
	if (myInfo.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_ASSIGNED) {
		staticWorkers = maxWorkers;
	}
	if (myInfo.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_DYNAMIC) {
		dynamicWorkers = maxWorkers;
	}
	if (myInfo.affinity & JOB_HANDLER_TESTER_WORKER_AFFINITY_MIXED) {
		dynamicWorkers = maxWorkers / 2;
		staticWorkers = maxWorkers / 2;
	}

	for (std::size_t i = 0; i < dynamicWorkers; ++i) {
		worker wrk(myHandler.make_worker());
		wrk.set_core_affinity(job_handler_detail::Worker_Auto_Affinity);
		wrk.set_queue_affinity(job_handler_detail::Worker_Auto_Affinity);
		wrk.set_execution_priority(4);
		wrk.set_name(std::string(std::string("DynamicWorker#") + std::to_string(i + 1)).c_str());
		wrk.activate();
	}
	for (std::size_t i = 0; i < staticWorkers; ++i) {
		worker wrk(myHandler.make_worker());
		wrk.set_core_affinity(job_handler_detail::Worker_Auto_Affinity);
		wrk.set_queue_affinity(i % job_handler_detail::Priority_Granularity);
		wrk.set_execution_priority(4);
		wrk.set_name(std::string(std::string("StaticWorker#") + std::to_string(i + 1)).c_str());
		wrk.activate();
	}
}

float job_handler_tester::run_consumption_parallel_test(std::size_t numInserts, void(*workfunc)(void))
{
	job root(myHandler.make_job(workfunc, 0));	
	job next(myHandler.make_job(workfunc, 0));
	next.add_dependency(root);
	next.enable();

	for (std::size_t i = 0; i < numInserts; ++i) {

		uint8_t children(rand() % 8);
		for (std::uint8_t j = 0; j < children; ++j, ++i) {
			job sibling(myHandler.make_job(workfunc, (j + i) % job_handler_detail::Priority_Granularity));
			sibling.add_dependency(next);
			sibling.enable();
		}

		job intermediate(std::move(next));
		next = myHandler.make_job(workfunc, i % job_handler_detail::Priority_Granularity);
		next.add_dependency(intermediate);
		next.enable();
	}

	job end(myHandler.make_job(workfunc));
	end.add_dependency(next);
	end.enable();

	timer<float> timer;

	root.enable();
	end.wait_for_finish();

	return timer.get();
}

float job_handler_tester::run_construction_parallel_test(std::size_t numInserts, void(*workfunc)(void))
{
	numInserts; workfunc;

	// Hmm still something about concurrency when depending on other job during possible moving operation... Argh. Needs ASP ?
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

float job_handler_tester::run_mixed_parallel_test(std::size_t numInserts, void(*workfunc)(void))
{
	numInserts; workfunc;

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

float job_handler_tester::run_consumption_strand_test(std::size_t numInserts, void(*workfunc)(void))
{
	numInserts; workfunc;

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

}