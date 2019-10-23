#pragma once

#include <gdul\WIP_job_handler\job_handler_commons.h>

namespace gdul
{
namespace job_handler_detail
{
class worker_impl;
}
class worker
{
public:
	worker(job_handler_detail::worker_impl* impl);
	worker(const worker&) = default;
	worker(worker&&) = default;
	worker& operator=(const worker&) = default;
	worker& operator=(worker&&) = default;

	~worker();

	// Sets core affinity. job_handler_detail::Worker_Auto_Affinity represents automatic setting
	void set_core_affinity(std::uint8_t core = job_handler_detail::Worker_Auto_Affinity);

	// Sets which job queue to consume from. job_handler_detail::Worker_Auto_Affinity represents
	// dynamic runtime selection
	void set_queue_affinity(std::uint8_t queue = job_handler_detail::Worker_Auto_Affinity);

	// Thread priority as defined in WinBase.h
	void set_execution_priority(std::int32_t priority);

	// Thread name
	void set_name(const char* name);

	void enable();

	bool deactivate();

	bool is_retired() const;

private:
	friend class job_handler;

	job_handler_detail::worker_impl* myImpl;
};
}