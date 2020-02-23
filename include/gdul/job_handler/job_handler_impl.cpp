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

#include <string>
#include <thread>
#include <gdul/job_handler/job_handler_impl.h>
#include <gdul/job_handler/job_handler.h>
#include <gdul/job_handler/chunk_allocator.h>

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
thread_local job_handler_impl::tl_container job_handler_impl::t_items{ &job_handler_impl::t_items.m_implicitWorker };

job_handler_impl::job_handler_impl()
	: m_jobImplChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobNodeChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_batchJobChunkPool(Batch_Job_Allocator_Block_Size, m_mainAllocator)
	, m_workerIndices(0)
	, m_workerCount(0)
{
}

job_handler_impl::job_handler_impl(allocator_type & allocator)
	: m_mainAllocator(allocator)
	, m_jobImplChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_jobNodeChunkPool(jh_detail::Job_Impl_Allocator_Block_Size, m_mainAllocator)
	, m_batchJobChunkPool(Batch_Job_Allocator_Block_Size, m_mainAllocator)
	, m_workerIndices(0)
	, m_workerCount(0)
{
}


job_handler_impl::~job_handler_impl()
{
	retire_workers();
}

void job_handler_impl::retire_workers()
{
	m_workerCount.store(0, std::memory_order_relaxed);
	const std::uint16_t workers(m_workerIndices.exchange(0, std::memory_order_seq_cst));

	for (size_t i = 0; i < workers; ++i) {
		m_workers[i].disable();
	}
}

worker job_handler_impl::make_worker()
{
	gdul::delegate<void()> entryPoint(&job_handler_impl::work, this);

	worker w(make_worker(entryPoint));
	w.set_name("worker");

	m_workerCount.fetch_add(1, std::memory_order_relaxed);

	return w;
}
worker job_handler_impl::make_worker(gdul::delegate<void()> entryPoint)
{
	const std::uint16_t index(m_workerIndices.fetch_add(1, std::memory_order_relaxed));

	const std::uint8_t autoCoreAffinity(static_cast<std::uint8_t>(index % std::thread::hardware_concurrency()));	

	std::thread thread(&job_handler_impl::launch_worker, this, index);

	jh_detail::worker_impl impl(std::move(thread), m_mainAllocator);
	impl.set_core_affinity(autoCoreAffinity);

	impl.set_entry_point(std::move(entryPoint));

	m_workers[index] = std::move(impl);

	return worker(&m_workers[index]);
}
job job_handler_impl::make_job(delegate<void()>&& workUnit)
{
	job_impl_allocator alloc(&m_jobImplChunkPool);
	const job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this
			));

	return job(jobImpl);
}

std::size_t job_handler_impl::internal_worker_count() const noexcept
{
	return m_workerCount.load(std::memory_order_relaxed);
}
std::size_t job_handler_impl::external_worker_count() const noexcept
{
	return m_workerIndices.load(std::memory_order_relaxed) - m_workerCount.load(std::memory_order_relaxed);
}
std::size_t job_handler_impl::active_job_count() const noexcept
{
	std::size_t accum(0);

	for (std::uint8_t i = 0; i < job_queue_count; ++i) {
		accum += m_jobQueues[i].size();
	}

	return accum;
}
concurrent_object_pool<job_node_chunk_rep, allocator_type>* job_handler_impl::get_job_node_chunk_pool() noexcept
{
	return &m_jobNodeChunkPool;
}

concurrent_object_pool<batch_job_chunk_rep, allocator_type>* job_handler_impl::get_batch_job_chunk_pool() noexcept
{
	return &m_batchJobChunkPool;
}

void job_handler_impl::enqueue_job(job_impl_shared_ptr job)
{
	const std::uint8_t target(job->get_target_queue());

	m_jobQueues[target].push(std::move(job));
}
bool job_handler_impl::try_consume_from_once(job_queue consumeFrom)
{
	job_handler_impl::job_impl_shared_ptr jb;

	if (m_jobQueues[consumeFrom].try_pop(jb)) {
		job swap(std::move(job_handler::this_job));

		job_handler::this_job = job(std::move(jb));

		job_handler::this_job.m_impl->operator()();

		job_handler::this_job = std::move(swap);

		return true;
	}
	
	return false;
}
void job_handler_impl::launch_worker(std::uint16_t index) noexcept
{
	t_items.this_worker_impl = &m_workers[index];
	job_handler::this_worker = worker(t_items.this_worker_impl);

	while (!t_items.this_worker_impl->is_enabled()) {
		t_items.this_worker_impl->idle();
	}

	t_items.this_worker_impl->refresh_sleep_timer();

	t_items.this_worker_impl->on_enable();
	t_items.this_worker_impl->entry_point();
	t_items.this_worker_impl->on_disable();
}
void job_handler_impl::work()
{
	while (t_items.this_worker_impl->is_active()) {
		job_handler::this_job = job(fetch_job());

		if (job_handler::this_job) {
			(*job_handler::this_job.m_impl)();

			job_handler::this_job = job();

			t_items.this_worker_impl->refresh_sleep_timer();
		}
		else{
			t_items.this_worker_impl->idle();
		}

	}
}

job_handler_impl::job_impl_shared_ptr job_handler_impl::fetch_job()
{
	const uint8_t queueIndex(t_items.this_worker_impl->get_queue_target());

	job_handler_impl::job_impl_shared_ptr out(nullptr);

	for (uint8_t i = 0; i < t_items.this_worker_impl->get_fetch_retries(); ++i) {

		if (m_jobQueues[queueIndex].try_pop(out)) {
			return out;
		}
	}

	return out;
}
}
}