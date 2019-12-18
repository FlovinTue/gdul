#pragma once

#include <gdul/WIP_job_handler/job_handler_commons.h>
#include <gdul/WIP_job_handler/job_interface.h>
#include <gdul/WIP_job_handler/scatter_job_impl_abstr.h>

namespace gdul {
class job;

namespace jh_detail {
}

class scatter_job
{
public:
	scatter_job() : m_impl(nullptr), m_storage{} {}
	~scatter_job() { m_impl->~job_interface(); }

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
	operator bool() const noexcept {
		m_impl;
	}

private:
	friend class job_handler;

	template <class T>
	scatter_job(shared_ptr<jh_detail::scatter_job_impl<T>>&& job) {
		m_impl = new (&m_storage[0]) jh_detail::scatter_job_impl_abstr<T>(std::move(job));
	}

	jh_detail::job_interface* m_impl;
	std::uint8_t m_storage[sizeof(jh_detail::scatter_job_impl_abstr<void>)];
};
}