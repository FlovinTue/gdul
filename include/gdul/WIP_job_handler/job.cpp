// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <cassert>
#include <gdul/WIP_job_handler/Job.h>
#include <gdul/WIP_job_handler/job_handler_impl.h>
#include <gdul/WIP_job_handler/job_impl.h>

namespace gdul
{
job::job() noexcept
{
}
job::~job()
{
	assert(!(*this) || m_impl->is_enabled() && "Job destructor ran before enable was called");
}
job::job(job && other) noexcept
{
	operator=(std::move(other));
}
job::job(const job & other) noexcept
{
	operator=(other);
}
job & job::operator=(job && other) noexcept
{
	m_impl = std::move(other.m_impl);
	return *this;
}
job & job::operator=(const job & other) noexcept
{
	m_impl = other.m_impl;
	return *this;
}
void job::add_dependency(job & dependency)
{
	assert(m_impl && "Job not set");

	if (m_impl->try_add_dependencies(1)) {
		if (!dependency.m_impl->try_attach_child(m_impl)) {
			if (!m_impl->remove_dependencies(1)) {
				m_impl->get_handler()->enqueue_job(m_impl);
			}
		}
	}

}
void job::set_priority(std::uint8_t priority) noexcept
{
	m_impl->set_priority(priority);
}
void job::enable()
{
	assert(m_impl && "Job not set");

	if (m_impl->enable()) {
		m_impl->get_handler()->enqueue_job(m_impl);
	}
}
bool job::is_finished() const noexcept
{
	assert(m_impl && "Job not set");

	return m_impl->is_finished();
}
void job::wait_for_finish() noexcept
{
	assert(m_impl && "Job not set");

	while (!is_finished()) {
		jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
		jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
	}
}
job::job(gdul::shared_ptr<jh_detail::job_impl> impl) noexcept
	: m_impl(std::move(impl))
{
}
job::operator bool() const noexcept 
{
	return m_impl;
}
}
