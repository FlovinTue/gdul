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

#include "batch_job.h"
#include <gdul/job_handler/batch_job_impl.h>
#include <gdul/concurrent_object_pool/concurrent_object_pool.h>

namespace gdul {

batch_job::batch_job()
	: m_impl(nullptr)
{}
void batch_job::add_dependency(job & dependency)
{
	if (m_impl)
		m_impl->add_dependency(dependency);
}
void batch_job::set_target_queue(job_queue target) noexcept
{
	if (m_impl)
		m_impl->set_target_queue(target);
}
job_queue batch_job::get_target_queue() const noexcept 
{
	if (!m_impl)
		return jh_detail::Default_Job_Queue;

	return m_impl->get_target_queue();
}
bool batch_job::enable() noexcept
{
	return m_impl && m_impl->enable(m_impl);
}
bool batch_job::enable_locally_if_ready()
{
	return m_impl && m_impl->enable_locally_if_ready();
}
bool batch_job::is_finished() const noexcept
{
	return m_impl && m_impl->is_finished();
}
bool batch_job::is_ready() const noexcept
{
	return m_impl && m_impl->is_ready();
}
void batch_job::wait_until_finished() noexcept
{
	if (m_impl)
		m_impl->wait_until_finished();
}
void batch_job::wait_until_ready() noexcept
{
	if (m_impl)
		m_impl->wait_until_ready();
}
void batch_job::work_until_finished(job_queue consumeFrom)
{
	if (m_impl)
		m_impl->work_until_finished(consumeFrom);
}
void batch_job::work_until_ready(job_queue consumeFrom)
{
	if (m_impl)
		m_impl->work_until_ready(consumeFrom);
}
batch_job::operator bool() const noexcept
{
	return m_impl;
}
std::size_t batch_job::get_output_size() const noexcept
{
	if (!m_impl)
		return 0;

	return m_impl->get_output_size();
}
#if defined(GDUL_JOB_DEBUG)
constexpr_id batch_job::register_tracking_node(constexpr_id id, const char * name, const char* file, std::uint32_t line, bool)
{
	if (!m_impl)
		return constexpr_id::make<0>();

	return m_impl->register_tracking_node(id, name, file, line);
}
#endif
job & batch_job::get_endjob() noexcept
{
	return m_impl->get_endjob();
}

}
