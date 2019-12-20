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
#include <gdul/WIP_job_handler/base_job_abstr.h>
#include <gdul/WIP_job_handler/job_handler_impl.h>
#include <gdul/WIP_job_handler/base_job_impl.h>

namespace gdul
{
namespace jh_detail
{
base_job_abstr::base_job_abstr() noexcept
{
}
base_job_abstr::~base_job_abstr() noexcept
{
	assert(!(*this) || m_abstr->is_enabled() && "Job destructor ran before enable was called");
}
base_job_abstr::base_job_abstr(base_job_abstr && other) noexcept
{
	operator=(std::move(other));
}
base_job_abstr::base_job_abstr(const base_job_abstr & other) noexcept
{
	operator=(other);
}
base_job_abstr & base_job_abstr::operator=(base_job_abstr && other) noexcept
{
	m_abstr = std::move(other.m_abstr);
	return *this;
}
base_job_abstr & base_job_abstr::operator=(const base_job_abstr & other) noexcept
{
	m_abstr = other.m_abstr;
	return *this;
}
void base_job_abstr::add_dependency(job_interface& other)
{
	assert(m_abstr && "Job not set");

	if (m_abstr->try_add_dependencies(1))
	{
		if (!other.get_depender().m_abstr->try_attach_child(m_abstr))
		{
			if (!m_abstr->remove_dependencies(1))
			{
				m_abstr->get_handler()->enqueue_job(m_abstr);
			}
		}
	}

}
void base_job_abstr::set_priority(std::uint8_t priority) noexcept
{
	m_abstr->set_priority(priority);
}
void base_job_abstr::enable()
{
	assert(m_abstr && "Job not set");

	if (m_abstr->enable())
	{
		m_abstr->get_handler()->enqueue_job(m_abstr);
	}
}
bool base_job_abstr::is_finished() const noexcept
{
	assert(m_abstr && "Job not set");

	return m_abstr->is_finished();
}
void base_job_abstr::wait_for_finish() noexcept
{
	assert(m_abstr && "Job not set");

	while (!is_finished())
	{
		jh_detail::job_handler_impl::t_items.this_worker_impl->refresh_sleep_timer();
		jh_detail::job_handler_impl::t_items.this_worker_impl->idle();
	}
}
base_job_abstr::base_job_abstr(gdul::shared_ptr<jh_detail::base_job_impl> impl) noexcept
	: m_abstr(std::move(impl))
{
}
base_job_abstr::operator bool() const noexcept
{
	return m_abstr;
}
}
}