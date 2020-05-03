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

#include <gdul/job_handler/job_handler.h>
#include <gdul/job_handler/job_handler_impl.h>
#include <cassert>

namespace gdul
{
thread_local job job_handler::this_job(shared_ptr<jh_detail::job_impl>(nullptr));
thread_local worker job_handler::this_worker(&jh_detail::job_handler_impl::t_items.m_implicitWorker);

job_handler::job_handler()
	: job_handler(jh_detail::allocator_type())
{
}
job_handler::job_handler(jh_detail::allocator_type allocator)
	: m_allocator(allocator)
{

}
void job_handler::init() {
	jh_detail::allocator_type alloc(m_allocator);
	m_impl = gdul::allocate_shared<jh_detail::job_handler_impl>(alloc, alloc);
}
job_handler::~job_handler()
{
	retire_workers();
}
void job_handler::retire_workers()
{
	m_impl->retire_workers();
}
worker job_handler::make_worker()
{
	return m_impl->make_worker();
}
worker job_handler::make_worker(gdul::delegate<void()> entryPoint)
{
	return m_impl->make_worker(std::move(entryPoint));
}
std::size_t job_handler::internal_worker_count() const noexcept
{
	return m_impl->internal_worker_count();
}
std::size_t job_handler::external_worker_count() const noexcept
{
	return m_impl->external_worker_count();
}
std::size_t job_handler::active_job_count() const noexcept
{
	return m_impl->active_job_count();
}
job job_handler::make_job(gdul::delegate<void()> workUnit)
{
	return m_impl->make_job(std::move(workUnit));
}
pool_allocator<jh_detail::dummy_batch_type> job_handler::get_batch_job_allocator() const noexcept
{
	return m_impl->get_batch_job_allocator();
}
}
