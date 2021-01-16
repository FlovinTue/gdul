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

#include <gdul/execution/job_handler/worker/worker.h>
#include <gdul/execution/job_handler/worker/worker_impl.h>
#include <gdul/execution/job_handler/job_handler_impl.h>

namespace gdul
{
thread_local worker worker::this_worker(&jh_detail::job_handler_impl::t_items.m_implicitWorker);

worker::worker(jh_detail::worker_impl * impl)
	: m_impl(impl)
{
	assert(impl && "Null input to constructor");
}

worker::~worker()
{
}

void worker::add_assignment(job_queue* queue)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->add_assignment(queue);
}
const thread* worker::get_thread() const
{
	assert(m_impl && "Worker is not assigned");

	if (!m_impl)
		return nullptr;

	return &m_impl->get_thread();
}
thread* worker::get_thread()
{
	assert(m_impl && "Worker is not assigned");

	if (!m_impl)
		return nullptr;

	return &m_impl->get_thread();
}
void worker::enable()
{
	assert(m_impl && "Worker is not assigned");

	if (!m_impl)
		return;

	m_impl->enable();
}
bool worker::disable()
{
	assert(m_impl && "Worker is not assigned");

	return m_impl && m_impl->disable();
}
void worker::set_run_on_enable(delegate<void()> toCall)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_run_on_enable(std::move(toCall));
}
void worker::set_run_on_disable(delegate<void()> toCall)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_run_on_disable(std::move(toCall));
}
bool worker::is_active() const
{
	assert(m_impl && "Worker is not assigned");
	return m_impl && m_impl->is_active();
}
}
