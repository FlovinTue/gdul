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

#include <gdul\WIP_job_handler\job_handler_impl.h>
#include <string>
#include <thread>

#if defined(_WIN64) | defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul
{

#undef GetObject
namespace jh_detail
{
thread_local job job_handler_impl::this_job(nullptr);
thread_local worker job_handler_impl::this_worker(&job_handler_impl::ourImplicitWorker);
thread_local jh_detail::worker_impl* job_handler_impl::this_worker_impl(&job_handler_impl::ourImplicitWorker);
thread_local jh_detail::worker_impl job_handler_impl::ourImplicitWorker;

job_handler_impl::job_handler_impl()
	: m_jobImplChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobDependeeChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobImplAllocator(&m_jobImplChunkPool)
	, m_jobDependeeAllocator(&m_jobDependeeChunkPool)
	, m_workerCount(0)
{
}

job_handler_impl::job_handler_impl(allocator_type & allocator)
	: m_mainAllocator(allocator)
	, m_jobImplChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobDependeeChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobImplAllocator(&m_jobImplChunkPool)
	, m_jobDependeeAllocator(&m_jobDependeeChunkPool)
	, m_workerCount(0)
{
}


job_handler_impl::~job_handler_impl()
{
	retire_workers();
}

void job_handler_impl::retire_workers()
{
	const std::uint16_t workers(m_workerCount.exchange(0, std::memory_order_seq_cst));

	for (size_t i = 0; i < workers; ++i) {
		m_workers[i].deactivate();
	}
}

worker job_handler_impl::make_worker()
{
	const std::uint16_t index(m_workerCount.fetch_add(1, std::memory_order_relaxed));

	const std::uint8_t coreAffinity(static_cast<std::uint8_t>(index % std::thread::hardware_concurrency()));
	
	std::thread thread(&job_handler_impl::launch_worker, this, index);

	jh_detail::worker_impl impl(std::move(thread), m_mainAllocator, coreAffinity);

	m_workers[index] = std::move(impl);

	return worker(&m_workers[index]);
}

job job_handler_impl::make_job(job_delegate<void, void>&& del)
{
	const job_impl_shared_ptr jobImpl(make_shared<job_impl, job_impl_allocator>
		(
			m_jobImplAllocator,
			std::move(del),
			this
			));
	
	return job(jobImpl);
}

std::size_t job_handler_impl::num_workers() const noexcept
{
	return m_workerCount;
}

std::size_t job_handler_impl::num_enqueued() const noexcept
{
	std::size_t accum(0);

	for (std::uint8_t i = 0; i < jh_detail::Priority_Granularity; ++i) {
		accum += m_jobQueues[i].size();
	}

	return accum;
}

typename job_handler_impl::job_dependee_allocator job_handler_impl::get_job_dependee_allocator() const noexcept
{
	return m_jobDependeeAllocator;
}

void job_handler_impl::enqueue_job(job_impl_shared_ptr job)
{
	const std::uint8_t priority(job->get_priority());

	m_jobQueues[priority].push(std::move(job));
}

void job_handler_impl::launch_worker(std::uint16_t index) noexcept
{
	this_worker_impl = &m_workers[index];
	this_worker = worker(this_worker_impl);

	while (!this_worker_impl->is_enabled()) {
		this_worker_impl->idle();
	}

	this_worker_impl->refresh_sleep_timer();

	//myInitInfo.myOnThreadLaunch();

	work();

	//myInitInfo.myOnThreadExit();
}
void job_handler_impl::work()
{
	while (this_worker_impl->is_active()) {

		this_job = job(fetch_job());

		if (this_job.m_impl) {
			(*this_job.m_impl)();

			this_worker_impl->refresh_sleep_timer();

			continue;
		}

		this_worker_impl->idle();
	}
}

job_handler_impl::job_impl_shared_ptr job_handler_impl::fetch_job()
{
	const uint8_t queueIndex(this_worker_impl->get_queue_target());

	job_handler_impl::job_impl_shared_ptr out(nullptr);

	for (uint8_t i = 0; i < this_worker_impl->get_fetch_retries(); ++i) {

		const uint8_t index((queueIndex + i) % jh_detail::Priority_Granularity);

		if (m_jobQueues[index].try_pop(out)) {
			return out;
		}
	}

	return out;
}
}
}