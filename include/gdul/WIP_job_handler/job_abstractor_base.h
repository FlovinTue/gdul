#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>

namespace gdul
{
class job;

namespace jh_detail
{
class job_abstractor_base
{
public:
	virtual ~job_abstractor_base() = default;
	virtual void add_dependency(job& dependency) = 0;
	virtual void set_priority(std::uint8_t priority) noexcept = 0;
	virtual void enable() = 0;
	virtual bool is_finished() const noexcept = 0;
	virtual void wait_for_finish() noexcept = 0;
};
}
}
