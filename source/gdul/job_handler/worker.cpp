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

#include <gdul/job_handler/worker.h>
#include <gdul/job_handler/worker_impl.h>

namespace gdul
{
worker::worker(jh_detail::worker_impl * impl)
	: m_impl(impl)
{
	assert(impl && "Null input to constructor");
}

worker::~worker()
{
}

void worker::set_core_affinity(std::uint8_t core)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_core_affinity(core);
}

void worker::set_execution_priority(std::int32_t priority)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_execution_priority(priority);
}

void worker::set_name(const std::string& name)
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_name(name);
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
void worker::set_queue_consume_first(job_queue firstQueue) noexcept
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_queue_consume_first(firstQueue);
}
void worker::set_queue_consume_last(job_queue lastQueue) noexcept
{
	assert(m_impl && "Worker is not assigned");
	if (!m_impl)
		return;

	m_impl->set_queue_consume_last(lastQueue);
}
bool worker::is_active() const
{
	assert(m_impl && "Worker is not assigned");
	return m_impl && m_impl->is_active();
}
}