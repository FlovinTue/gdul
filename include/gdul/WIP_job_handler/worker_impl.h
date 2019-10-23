#pragma once

#pragma warning(push)
#pragma warning(disable : 4324)

#include <chrono>
#include <atomic>
#include <thread>

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul {

class job_handler;

namespace job_handler_detail
{
class alignas(64) worker_impl
{
public:
	worker_impl();
	worker_impl(std::thread&& thread, std::uint8_t coreAffinity);
	~worker_impl();

	worker_impl& operator=(worker_impl&& other);

	void set_core_affinity(std::uint8_t core);
	void set_queue_affinity(std::uint8_t queue);
	void set_execution_priority(std::uint32_t priority);
	void set_sleep_threshhold(std::uint16_t ms);
	void set_name(const char* name);

	void enable();

	bool deactivate();

	void refresh_sleep_timer();

	bool is_sleepy() const;
	bool is_active() const;
	bool is_enabled() const;

	void idle();

	std::uint8_t get_queue_target();
	std::uint8_t get_fetch_retries() const;

private:
	std::thread myThread;

	thread_handle myThreadHandle;

	uint32_t myNativeThreadId;

	std::size_t myPriorityDistributionIteration;

	std::chrono::high_resolution_clock::time_point myLastJobTimepoint;

	std::uint16_t mySleepThreshhold;

	std::chrono::high_resolution_clock mySleepTimer;

	std::uint8_t myAutoCoreAffinity;
	std::uint8_t myCoreAffinity;
	std::uint8_t myQueueAffinity;

	std::atomic_bool myIsRunning;
	std::atomic_bool myIsActive;
};
}
}
#pragma warning(pop)