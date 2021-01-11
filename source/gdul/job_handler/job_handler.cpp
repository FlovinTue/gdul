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
#include <gdul/job_handler/tracking/job_graph.h>
#include <cassert>

namespace gdul
{
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
	shutdown();
}
void job_handler::shutdown()
{
	m_impl->shutdown();
}
worker job_handler::make_worker()
{
	return m_impl->make_worker();
}
#if defined (GDUL_JOB_DEBUG)
void job_handler::dump_job_graph()
{
	dump_job_graph("");
}
void job_handler::dump_job_time_sets()
{
	dump_job_time_sets("");
}
void job_handler::dump_job_graph(const char* location)
{
	m_impl->dump_job_graph(location);
}
void job_handler::dump_job_time_sets(const char* location)
{
	m_impl->dump_job_time_sets(location);
}
#endif
job job_handler::_redirect_make_job(std::size_t physicalId, [[maybe_unused]] const char* dbgFile, [[maybe_unused]] std::uint32_t line, delegate<void()> workUnit, job_queue* target, std::size_t variationId, [[maybe_unused]] const char* dbgName)
{
#if defined (GDUL_JOB_DEBUG)
	return m_impl->make_job_internal(std::move(workUnit), target, physicalId, variationId, dbgName, dbgFile, line);
#else
	return m_impl->make_job_internal(std::move(workUnit), target, physicalId, variationId);
#endif
}
job job_handler::_redirect_make_job(std::size_t physicalId, [[maybe_unused]] const char* dbgFile, [[maybe_unused]] std::uint32_t line, delegate<void()> workUnit, job_queue* target, [[maybe_unused]] const char* dbgName)
{
#if defined (GDUL_JOB_DEBUG)
	return m_impl->make_job_internal(std::move(workUnit), target, physicalId, 0, dbgName, dbgFile, line);
#else
	return m_impl->make_job_internal(std::move(workUnit), target, physicalId, 0);
#endif
}
std::size_t job_handler::worker_count() const noexcept
{
	return m_impl->worker_count();
}
pool_allocator<jh_detail::dummy_batch_type> job_handler::get_batch_job_allocator() const noexcept
{
	return m_impl->get_batch_job_allocator();
}
jh_detail::job_info* job_handler::get_job_info(std::size_t physicalId, std::size_t variationId, [[maybe_unused]] const char* dbgName, [[maybe_unused]] const char* dbgFile, [[maybe_unused]] std::uint32_t line)
{
#if defined (GDUL_JOB_DEBUG)
	return m_impl->get_job_graph().get_job_info(physicalId, variationId, dbgName, dbgFile, line);
#else
	return m_impl->get_job_graph().get_job_info(physicalId, variationId);
#endif
}
}
