// Copyright(c) 2020 Flovin Michaelsen
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
#include <gdul/job_handler/Job.h>
#include <gdul/job_handler/job_handler_impl.h>
#include <gdul/job_handler/job_impl.h>
#include <gdul/job_handler/batch_job.h>
#include <gdul/job_handler/globals.h>
#include <gdul/job_handler/job_queue.h>

namespace gdul
{
thread_local job job::this_job(shared_ptr<jh_detail::job_impl>(nullptr));

job::job() noexcept
{
}
job::job(job&& other) noexcept
{
	operator=(std::move(other));
}
job::job(const job& other) noexcept
{
	operator=(other);
}
job& job::operator=(job&& other) noexcept
{
	m_impl = std::move(other.m_impl);
	return *this;
}
job& job::operator=(const job& other) noexcept
{
	m_impl = other.m_impl;
	return *this;
}
void job::add_dependency(job& dependency)
{
	if (!m_impl)
		return;

	if (m_impl->try_add_dependencies(1)) {
		if (!dependency.m_impl->try_attach_child(m_impl)) {
			if (!m_impl->remove_dependencies(1)) {
				m_impl->get_target()->submit_job(m_impl);
			}
		}
	}

}
void job::add_dependency(batch_job& dependency)
{
	add_dependency(dependency.get_endjob());
}

bool job::enable() noexcept
{
	if (m_impl) {
		const jh_detail::enable_result result(m_impl->enable());

		if (result & jh_detail::enable_result_enqueue) {
			m_impl->get_target()->submit_job(m_impl);
		}

		return result & jh_detail::enable_result_enabled;
	}
	return false;
}
bool job::enable_locally_if_ready() noexcept
{
	if (m_impl && m_impl->enable_if_ready()) {

		GDUL_JOB_DEBUG_CONDTIONAL(m_impl->on_enqueue())

		m_impl->operator()();

		return true;
	}
	return false;
}
bool job::is_ready() const noexcept
{
	if (m_impl)
		return m_impl->is_ready();

	return false;
}
bool job::is_finished() const noexcept
{
	return m_impl && m_impl->is_finished();
}
void job::wait_until_finished() noexcept
{
	if (!m_impl)
		return;

	m_impl->wait_until_finished();
}
void job::wait_until_ready() noexcept
{
	if (!m_impl)
		return;

	m_impl->wait_until_ready();
}
void job::work_until_finished(job_queue* consumeFrom)
{
	assert(consumeFrom && "Null ptr");

	if (m_impl)
		m_impl->work_until_finished(consumeFrom);
}
void job::work_until_ready(job_queue* consumeFrom)
{
	assert(consumeFrom && "Null ptr");

	if (m_impl)
		m_impl->work_until_ready(consumeFrom);
}
job::job(gdul::shared_ptr<jh_detail::job_impl> impl) noexcept
	: m_impl(std::move(impl))
{
}
job::operator bool() const noexcept
{
	return m_impl;
}
float job::priority() const noexcept
{
	return m_impl->get_priority();
}
#if defined(GDUL_JOB_DEBUG)
constexpr_id job::register_tracking_node(constexpr_id id, const char* name, const char* file, std::uint32_t line, bool batchSub)
{
	if (!m_impl)
		return constexpr_id::make<0>();

	return m_impl->register_tracking_node(id, name, file, line, batchSub);
}
#endif
}
