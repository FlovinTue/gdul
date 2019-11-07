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

	worker_impl& operator=(worker_impl&& other) noexcept;

	void set_core_affinity(std::uint8_t core);
	void set_queue_affinity(std::uint8_t queue);
	void set_execution_priority(std::uint32_t priority);
	void set_sleep_threshhold(std::uint16_t ms);
	void set_name(const char* name);

	void activate();

	bool deactivate();

	void refresh_sleep_timer();

	bool is_sleepy() const;
	bool is_active() const;
	bool is_enabled() const;

	void idle();

	std::uint8_t get_queue_target();
	std::uint8_t get_fetch_retries() const;

private:
	std::thread m_thread;

	thread_handle m_threadHandle;
	
	std::size_t m_priorityDistributionIteration;

	std::chrono::high_resolution_clock::time_point m_lastJobTimepoint;

	std::uint16_t m_sleepThreshhold;

	std::chrono::high_resolution_clock m_sleepTimer;

	std::uint8_t m_autoCoreAffinity;
	std::uint8_t m_coreAffinity;
	std::uint8_t m_queueAffinity;

	std::atomic_bool m_isEnabled;
	std::atomic_bool m_isActive;
};
}
}
#pragma warning(pop)