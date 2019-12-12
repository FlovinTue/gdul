#pragma once



#include <gdul/WIP_job_handler/job_handler_commons.h>


namespace gdul
{
class job;

namespace jh_detail
{
template <class T>
class job_interface
{
public:
	void add_dependency(job& dependency){
		(T*)this->something();
	}

	void set_priority(std::uint8_t priority) noexcept;

	void wait_for_finish() noexcept;

	operator bool() const noexcept;

	void enable();
	bool is_finished() const;
};
}
}