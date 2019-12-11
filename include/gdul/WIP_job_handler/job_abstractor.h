#pragma once


#include <gdul/WIP_job_handler/job_abstractor_base.h>
#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>

namespace gdul
{
namespace jh_detail
{
template <class JobImpl>
class job_abstractor : public job_abstractor_base
{
public:
	job_abstractor(gdul::shared_ptr<JobImpl> impl){
		m_impl = std::move(impl);
	}
	void add_dependency(job& dependency) override{
		m_impl->add_dependency(dependency);
	}
	void set_priority(std::uint8_t priority) noexcept override{
		m_impl->set_priority(priority);
	}
	void enable(){
		m_impl->enable();
	}
	bool is_finished() const noexcept override{
		return m_impl->is_finished();
	}
	void wait_for_finish() noexcept override{
		m_impl->wait_for_finish();
	}
	void is_enabled() = 0 const noexcept override{
		return m_impl->is_enabled();
	}
private:
	gdul::shared_ptr<JobImpl> m_impl;
};
}
}