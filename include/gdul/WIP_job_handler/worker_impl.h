#pragma once

#pragma warning(push)
#pragma warning(disable : 4324)

#include <chrono>
#include <atomic>
#include <thread>

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {
namespace job_handler_detail
{
class job_handler;
class alignas(64) worker_impl
{
public:
	worker_impl();
	worker_impl(std::thread&& thread, std::uint8_t coreAffinity);
	~worker_impl();

	worker_impl& operator=(worker_impl&& other);

	// Sets core affinity. job_handler_detail::Worker_Auto_Affinity represents automatic setting
	void set_core_affinity(std::uint8_t core);

	// Sets which job queue to consume from. job_handler_detail::Worker_Auto_Affinity represents
	// dynamic runtime selection
	void set_queue_affinity(std::uint8_t queue);

	// Thread priority as defined in WinBase.h
	void set_execution_priority(std::uint32_t priority);

	void set_sleep_threshhold(std::uint16_t ms);

	void set_thread_handle(HANDLE handle);

	bool retire();

	void refresh_sleep_timer();

	bool is_sleepy() const;
	bool is_retired() const;

	void set_name(const char* name);

	void idle();

	std::uint8_t get_queue_affinity();

private:
	std::thread myThread;

	HANDLE myThreadHandle;

	std::size_t myPriorityDistributionIteration;

	std::chrono::high_resolution_clock::time_point myLastJobTimepoint;

	std::uint16_t mySleepThreshhold;

	std::chrono::high_resolution_clock mySleepTimer;

	std::uint8_t myAutoCoreAffinity;
	std::uint8_t myCoreAffinity;
	std::uint8_t myQueueAffinity;

	std::atomic_bool myIsRunning;
};
}
}
#pragma warning(pop)