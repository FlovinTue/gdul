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

#include <gdul\WIP_job_handler\job_impl.h>
#include <gdul\WIP_job_handler\job_handler_impl.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>

namespace gdul
{
namespace jh_detail
{
job_impl::job_impl()
	: job_impl(delegate<void()>([]() {}), nullptr)
{
}
job_impl::job_impl(delegate<void()>&& workUnit, job_handler_impl* handler)
	: m_workUnit(std::forward<delegate<void()>>(workUnit))
	, m_finished(false)
	, m_enabled(false)
	, m_firstDependee(nullptr)
	, m_priority(Default_Job_Priority)
	, m_handler(handler)
	, m_dependencies(Job_Max_Dependencies)
{
}
job_impl::~job_impl()
{
}

void job_impl::operator()()
{
	m_workUnit();

	m_finished.store(true, std::memory_order_seq_cst);

	detach_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	typename job_handler_impl::job_dependee_allocator jobDependeeAlloc(m_handler->get_job_dependee_allocator());

	job_dependee_shared_ptr dependee(make_shared<job_dependee, typename job_handler_impl::job_dependee_allocator>(jobDependeeAlloc));
	dependee->m_job = std::move(child);

	job_dependee_shared_ptr firstDependee(nullptr);
	job_dependee_raw_ptr rawRep(nullptr);
	do {
		firstDependee = m_firstDependee.load(std::memory_order_relaxed);
		rawRep = firstDependee;

		dependee->m_sibling = std::move(firstDependee);

		if (m_finished.load(std::memory_order_seq_cst)) {
			return false;
		}

	} while (!m_firstDependee.compare_exchange_strong(rawRep, std::move(dependee), std::memory_order_relaxed, std::memory_order_relaxed));

	return true;
}
std::uint8_t job_impl::get_priority() const
{
	return m_priority;
}
void job_impl::set_priority(std::uint8_t priority)
{
	m_priority = priority;
}
std::uint32_t job_impl::add_dependencies(std::uint32_t n)
{
	std::uint32_t result(m_dependencies.fetch_add(n, std::memory_order_acq_rel));
	return result - n;
}
std::uint32_t job_impl::remove_dependencies(std::uint32_t n)
{
	std::uint32_t result(m_dependencies.fetch_sub(n, std::memory_order_acq_rel));
	return result - n;
}
bool job_impl::enable()
{
	if (!m_enabled.exchange(true, std::memory_order_relaxed)){
		return !remove_dependencies(Job_Max_Dependencies);
	}
	return false;
}
job_handler_impl * job_impl::get_handler() const
{
	return m_handler;
}
bool job_impl::is_finished() const
{
	return m_finished.load(std::memory_order_relaxed);
}
bool job_impl::is_enabled() const
{
	return m_enabled.load(std::memory_order_relaxed);
}
void job_impl::detach_children()
{
	job_dependee_shared_ptr dependee(m_firstDependee.exchange(nullptr, std::memory_order_relaxed));

	while (dependee) {
		job_dependee_shared_ptr next(std::move(dependee->m_sibling));

		if (!dependee->m_job->remove_dependencies(1)) {
			job_handler_impl* const nativeHandler(dependee->m_job->get_handler());
			nativeHandler->enqueue_job(std::move(dependee->m_job));
		}

		dependee = std::move(next);
	}
}
}
}
