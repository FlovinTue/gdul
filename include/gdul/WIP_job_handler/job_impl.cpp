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
#include <gdul\WIP_job_handler\job_handler.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>

namespace gdul
{
namespace jh_detail
{


job_impl::~job_impl()
{
	m_callable->~callable_base();

	if ((void*)m_callable != (void*)&m_storage[0]) {
		m_allocFields.m_allocator.deallocate(m_allocFields.m_callableBegin, m_allocFields.m_allocated);
		m_allocFields.m_allocator.~allocator_type();
	}
	m_callable = nullptr;

	std::uninitialized_fill_n(m_storage, Callable_Max_Size_No_Heap_Alloc, std::uint8_t(0));
}

void job_impl::operator()()
{
	(*m_callable)();

	m_finished.store(true, std::memory_order_seq_cst);

	detach_children();
}
bool job_impl::try_attach_child(job_impl_shared_ptr child)
{
	job_dependee_shared_ptr dependee(make_shared<job_dependee>());
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
void job_impl::add_dependencies(std::uint32_t n)
{
	m_dependencies.fetch_add(n, std::memory_order_relaxed);
}
std::uint32_t job_impl::remove_dependencies(std::uint32_t n)
{
	std::uint32_t result(m_dependencies.fetch_sub(n, std::memory_order_acq_rel));
	return result - n;
}
bool job_impl::enable()
{
	return !remove_dependencies(Job_Max_Dependencies);
}
job_handler * job_impl::get_handler() const
{
	return m_handler;
}
bool job_impl::is_finished() const
{
	return m_finished.load(std::memory_order_relaxed);
}
void job_impl::detach_children()
{
	job_dependee_shared_ptr dependee(m_firstDependee.exchange(nullptr, std::memory_order_relaxed));

	while (dependee) {
		job_dependee_shared_ptr next(nullptr);
		// hmm
		//if (!dependee->m_job->remove_dependencies(1)) {
		//	next = child->m_firstSibling.unsafe_exchange(nullptr, std::memory_order_relaxed);
		//	job_handler* const nativeHandler(child->get_handler());
		//	nativeHandler->enqueue_job(std::move(child));
		//}

		//child = std::move(next);
	}
}
}
}
