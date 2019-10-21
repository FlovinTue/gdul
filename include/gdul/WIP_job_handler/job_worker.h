#pragma once

#include <chrono>
#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {

enum class job_worker_affinity_mode
{
	automatic,
	manual,
};
class job_handler;
class alignas(64) job_worker
{
public:
	job_worker();
	~job_worker();

	void set_queue_affinity_mode(job_worker_affinity_mode mode);
	void set_core_affinity_mode(job_worker_affinity_mode mode);

	void set_core_affinity(std::uint8_t core);
	void set_queue_affinity(std::uint8_t queue);

	// Thread priority as defined in WinBase.h
	void set_execution_priority(uint32_t priority);

	void retire();

	void refresh_sleep_timer();
	bool sleep_check() const;
private:
	std::thread myThread;

	std::size_t myPriorityDistributionIteration;

	std::chrono::high_resolution_clock mySleepTimer;
	std::chrono::high_resolution_clock::time_point myLastJobTimepoint;

	std::uint8_t myAutoCoreAffinity; 
	std::uint8_t myCoreAffinity;
	std::uint8_t myQueueAffinity;

	std::atomic_bool myIsRunning;
};

}