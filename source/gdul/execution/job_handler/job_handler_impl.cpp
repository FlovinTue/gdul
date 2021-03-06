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
#include <gdul/execution/job_handler/job_handler_impl.h>
#include <gdul/execution/job_handler/job_handler.h>
#include <gdul/execution/thread/thread.h>

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
	: m_jobImplMemPool()
	, m_jobNodeMemPool()
	, m_batchJobMemPool()
	, m_jobGraph(allocator)
	, m_workers{}
	, m_workerIndices(0)
	, m_mainAllocator(allocator)
{
	constexpr std::size_t jobImplAllocSize(allocate_shared_size<job_impl, pool_allocator<std::uint8_t>>());
	constexpr std::size_t jobNodeAllocSize(allocate_shared_size<job_node, pool_allocator<std::uint8_t>>());
	constexpr std::size_t batchJobAllocSize(allocate_shared_size<dummy_batch_type, pool_allocator<std::uint8_t>>());

	m_jobImplMemPool.init<jobImplAllocSize, alignof(job_impl)>(JobPoolInitSize, 1, m_mainAllocator);
	m_jobNodeMemPool.init<jobNodeAllocSize, alignof(job_node)>(JobPoolInitSize + jh_detail::BatchJobPoolInitSize, 1, m_mainAllocator);
	m_batchJobMemPool.init<batchJobAllocSize, alignof(dummy_batch_type)>(BatchJobPoolInitSize, 1, m_mainAllocator);
}


job_handler_impl::~job_handler_impl()
{
	shutdown();
}

void job_handler_impl::shutdown()
{
	const std::uint16_t workers(m_workerIndices.exchange(0, std::memory_order_seq_cst));

	for (size_t i = 0; i < workers; ++i) {
		m_workers[i].disable();
	}
}


worker job_handler_impl::make_worker()
{
	const std::uint16_t index(m_workerIndices.fetch_add(1, std::memory_order_relaxed));

	thread thread(&job_handler_impl::launch_worker, this, index);

	jh_detail::worker_impl impl(std::move(thread));

	m_workers[index] = std::move(impl);

	return worker(&m_workers[index]);
}
#if defined (GDUL_JOB_DEBUG)
job job_handler_impl::make_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t physicalId, std::size_t variationId, const std::string_view& name, const std::string_view& file, std::uint32_t line)
{
	pool_allocator<std::uint8_t> alloc(m_jobImplMemPool.create_allocator<std::uint8_t>());

	job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this,
			target,
			m_jobGraph.get_job_info(physicalId, variationId, name, file, line)));

	return job(jobImpl);
}
job job_handler_impl::make_sub_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t batchId, std::size_t variationId, const std::string_view& name)
{
	pool_allocator<std::uint8_t> alloc(m_jobImplMemPool.create_allocator<std::uint8_t>());

	job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this,
			target,
			m_jobGraph.get_sub_job_info(batchId, variationId, name)));

	return job(jobImpl);
}
#else
job job_handler_impl::make_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t physicalId, std::size_t variationId)
{
	pool_allocator<std::uint8_t> alloc(m_jobImplMemPool.create_allocator<std::uint8_t>());

	job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this,
			target,
			m_jobGraph.get_job_info(physicalId, variationId)));

	return job(jobImpl);
}
job job_handler_impl::make_sub_job_internal(delegate<void()>&& workUnit, job_queue* target, std::size_t batchId, std::size_t variationId)
{
	pool_allocator<std::uint8_t> alloc(m_jobImplMemPool.create_allocator<std::uint8_t>());

	job_impl_shared_ptr jobImpl(gdul::allocate_shared<job_impl>
		(
			alloc,
			std::forward<delegate<void()>>(workUnit),
			this,
			target,
			m_jobGraph.get_sub_job_info(batchId, variationId)));

	return job(jobImpl);
}
#endif
std::size_t job_handler_impl::worker_count() const noexcept
{
	return m_workerIndices.load(std::memory_order_relaxed);
}

job_graph& job_handler_impl::get_job_graph()
{
	return m_jobGraph;
}

pool_allocator<std::uint8_t> job_handler_impl::get_job_node_allocator() const noexcept
{
	return m_jobNodeMemPool.create_allocator<std::uint8_t>();
}
pool_allocator<std::uint8_t> job_handler_impl::get_batch_job_allocator() const noexcept
{
	return m_batchJobMemPool.create_allocator<std::uint8_t>();
}
#if defined(GDUL_JOB_DEBUG)
void job_handler_impl::dump_job_graph(const std::string_view& location)
{
	m_jobGraph.dump_job_graph(location);
}
void job_handler_impl::dump_job_time_sets(const std::string_view& location)
{
	m_jobGraph.dump_job_time_sets(location);
}
#endif
void job_handler_impl::launch_worker(std::uint16_t index) noexcept
{
	t_items.this_worker_impl = &m_workers[index];
	worker::this_worker = worker(t_items.this_worker_impl);

	while (!t_items.this_worker_impl->is_enabled()) {
		t_items.this_worker_impl->idle();
	}

	t_items.this_worker_impl->refresh_sleep_timer();

	t_items.this_worker_impl->on_enable();
	t_items.this_worker_impl->work();
	t_items.this_worker_impl->on_disable();
}
}
}