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

#include <string>
#include <thread>
#include <gdul/job_handler/job_handler_impl.h>
#include <gdul/job_handler/job_handler.h>

#if defined(_WIN64) | defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace gdul
{
namespace jh_detail
{
thread_local job_handler_impl::tl_container job_handler_impl::t_items{ &job_handler_impl::t_items.m_implicitWorker };

job_handler_impl::job_handler_impl()
	: job_handler_impl(allocator_type())
{

}

job_handler_impl::job_handler_impl(allocator_type allocator)
	: m_mainAllocator(allocator)
	, m_workerIndices(0)
	, m_workerCount(0)
{
	constexpr std::size_t jobImplAllocSize(allocate_shared_size<job_impl, pool_allocator<job_impl>>());
	constexpr std::size_t jobNodeAllocSize(allocate_shared_size<job_node, pool_allocator<job_node>>());
	constexpr std::size_t batchJobAllocSize(allocate_shared_size<dummy_batch_type, pool_allocator<dummy_batch_type>>());

	m_jobImplMemPool.init<jobImplAllocSize, alignof(job_impl)>(Job_Pool_Init_Size, m_mainAllocator);
	m_jobNodeMemPool.init<jobNodeAllocSize, alignof(job_node)>(Job_Pool_Init_Size + jh_detail::Batch_Job_Pool_Init_Size, m_mainAllocator);
	m_batchJobMemPool.init<batchJobAllocSize, alignof(dummy_batch_type)>(Batch_Job_Pool_Init_Size, m_mainAllocator);
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
	pool_allocator<job_impl> alloc(m_jobImplMemPool.create_allocator<job_impl>());
	const job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this
			));

	return job(jobImpl);
}

std::size_t job_handler_impl::worker_count() const noexcept
{
	return m_workerIndices.load(std::memory_order_relaxed);
}

pool_allocator<job_node> job_handler_impl::get_job_node_allocator() const noexcept
{
	return m_jobNodeMemPool.create_allocator<job_node>();
}
pool_allocator<typename dummy_batch_type> job_handler_impl::get_batch_job_allocator() const noexcept
{
	return m_batchJobMemPool.create_allocator<dummy_batch_type>();
}
void job_handler_impl::enqueue_job(job_impl_shared_ptr job)
{
	const std::uint8_t target(job->get_target_queue());

	GDUL_JOB_DEBUG_CONDTIONAL(job->on_enqueue())

	m_jobQueues[target].push(std::move(job));
}
bool job_handler_impl::try_consume_from_once(job_queue consumeFrom)
{
	job_handler_impl::job_impl_shared_ptr jb;

	if (m_jobQueues[consumeFrom].try_pop(jb)) {
		
		consume_job(job(std::move(jb)));
		
		return true;
	}

	return false;
}
void job_handler_impl::launch_worker(std::uint16_t index) noexcept
{
	t_items.this_worker_impl = &m_workers[index];
	worker::this_worker = worker(t_items.this_worker_impl);

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

		if (job jb = fetch_job()) {
			consume_job(std::move(jb));
		}
		else {
			t_items.this_worker_impl->idle();
		}
	}
}

void job_handler_impl::consume_job(job && jb)
{
	job swap(std::move(job_handler::this_job));

	job_handler::this_job = job(std::move(jb));

	job_handler::this_job.m_impl->operator()();

	job_handler::this_job = std::move(swap);

	t_items.this_worker_impl->refresh_sleep_timer();
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