#pragma once

#include <gdul/WIP_job_handler/job_interface.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <gdul/WIP_job_handler/scatter_job_impl.h>

namespace gdul {
namespace jh_detail {
template <class T>
class scatter_job_impl_abstr : public job_interface
{
public:
	scatter_job_impl_abstr(shared_ptr<scatter_job_impl<T>>&& job) 
		: m_impl(std::move(job)) 
	{}

	void add_dependency(job& dependency) {
		m_impl->add_dependency(dependency);
	}
	void set_priority(std::uint8_t priority) noexcept {
		m_impl->set_priority(priority);
	}
	void enable() {
		m_impl->enable();
	}
	bool is_finished() const noexcept {
		return m_impl->is_finished();
	}
	void wait_for_finish() noexcept {
		m_impl->wait_for_finish();
	}

private:
	shared_ptr<scatter_job_impl<T>> m_impl;
};
}
}